#include <DHT.h>;
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <ThreeWire.h>                                // connect library ThreeWire
#include <RtcDS1302.h>                                // connect library RtcD

#define RELAY_PIN 10                                  // relay output

//Constants
#define DHTPIN 3                                      // what pin we're connected to
#define DHTTYPE DHT11                                 // DHT 11  (AM2302)
DHT dht(DHTPIN, DHTTYPE);                             // Initialize DHT sensor for normal 16mhz Arduino

//leds
#define RED_LED_PIN 13
#define BLUE_LED_PIN 12

//buzzer
#define BUZZER_PIN 8

//hc-sr04
#define HCSR_PING_PIN 7                               // Trigger Pin of Ultrasonic Sensor
#define HCSR_ECHO_PIN 6                               // Echo Pin of Ultrasonic Sensor
#define INTERFERANCE_MAX_LOW -1
#define INTERFERANCE_MAX_HIGH 1

//DS1302
ThreeWire myWire(4,5,2);                              // Specify output IO, SCLK, CE
RtcDS1302<ThreeWire> rtc(myWire);

//lcd1602
LiquidCrystal_I2C lcd(0x24,16,2);                     // Installing the display

//Variables
int chk;
float hum;  //Stores humidity value
float temp; //Stores temperature value

#define TEMP_MAX_MAX 38.0                             // Thermostat setting limits
#define TEMP_MIN_MAX 28.0                             // Thermostat setting limits

#define HUM_NORM 50.0

unsigned long timer_start_sensor;                     // global var init 0
unsigned long timer_start_warm;                       // global var init 0

boolean flag_temp = true;
boolean flag_relay = true;

int sensorVal;

long duration, inches, cm;                            //var for sensor
long tempValueInches = 0, tempValueCm = 0;
    
void setup() {
  Serial.begin(9600);
  dht.begin();
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);  
  pinMode(BLUE_LED_PIN, OUTPUT);  
  pinMode(BUZZER_PIN, OUTPUT);
  lcd.init();
  lcd.backlight();
  rtc.Begin();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__); // Copying date and time in compiled
  rtc.SetDateTime(compiled); // Установка времени
}

void loop() {
    if(millis() - timer_start_sensor > (unsigned long)1000){ // sensors will update their values every second 5*60*1000
      //Read data and store it to variables hum and temp
      hum = dht.readHumidity();
      temp = dht.readTemperature();
      
      if (isnan(hum) || isnan(temp)) { // Check if any reads failed and exit early (to try again).
        Serial.println("Failed to read from DHT sensor!");
        return;
      }
  
      timer_start_sensor = millis();

      lcd.setCursor(0, 0);
      lcd.print(temp);
      lcd.print("C  ");
      
      lcd.setCursor(10, 0);
      lcd.print(hum);
      lcd.print("%  ");

      deviationOfTemp();
      deviationOfHum();
      checkLargeDeviationOfTemp();
    }
      Serial.print("millis= ");
      Serial.print(millis()); 
      Serial.print("timer_start_warm= ");
      Serial.println(timer_start_warm); 

    if(flag_temp){
      if(millis() - timer_start_warm < (unsigned long)16*1000){ // chamber heating for 5 hours at 38 degrees -> 5*60*60*1000
          Serial.println("Time to bang bang! 5 hours");
          bang_bang(TEMP_MAX_MAX);
      }else {
        flag_temp=false;
        timer_start_warm = millis();
        Serial.print("flag_temp= "); 
        Serial.println(flag_temp); 
      }
    }else{
       if(millis() - timer_start_warm < (unsigned long)14*1000){ // chamber heating for 30 minutes at 28 degrees -> 30*60*1000
          Serial.println("Time to bang bang! 30 min");
          bang_bang(TEMP_MIN_MAX);
      }else {
        flag_temp=true;
        timer_start_warm = millis();
        Serial.print("flag_temp= "); 
        Serial.println(flag_temp);
    }
   }
      
    if(inches!=0&&cm!=0){
      tempValueInches = inches;
      tempValueCm = cm;
    }
    
    getDataToMoveInIncubator();
    
    if(inches!=0&&cm!=0&&tempValueInches!=0&&tempValueCm!=0){
      if(!(inches-tempValueInches>=INTERFERANCE_MAX_LOW && inches-tempValueInches<=INTERFERANCE_MAX_HIGH)){
        if(inches-tempValueInches!=0 || cm-tempValueCm!=0){ // if we find move in incubator activate buzzer
          tone (BUZZER_PIN, 1000); // turn on buzzer on 1000 Гц
          delay(1000); //60*1000
          noTone(BUZZER_PIN); // turn off buzzer
        }
      }
    }

    
    rtc.GetDateTime();
    RtcDateTime now = rtc.GetDateTime();

    lcd.setCursor(4, 1); 
    lcd.print(now.Hour()); 
    lcd.print(":"); 
    lcd.print(now.Minute()); 
    lcd.print(":"); 
    lcd.print(now.Second());
    
    delay(200);
}

void checkLargeDeviationOfTemp(){ //deviation of the temperature up to 20C to give a short sound signal
  int temp_value = flag_temp?TEMP_MAX_MAX:TEMP_MIN_MAX; // max value
  if(temp-20>=temp_value){
      tone (BUZZER_PIN, 600); // turn on buzzer on 600 Гц
      delay(200);
      noTone(BUZZER_PIN); // turn off buzzer
  }
   if(temp+20<temp_value){
      tone (BUZZER_PIN, 600); // turn on buzzer on 600 Гц
      delay(200);
      noTone(BUZZER_PIN); // turn off buzzer
  }
}

void deviationOfTemp(){ // deviation of temperature from the set by 5%
    int gpt = get_percent_temp();
    if(gpt>100){
      if(gpt-100>=5){
        digitalWrite(RED_LED_PIN, HIGH); // вкл светодиод что отвечает за температуру
        delay(50);
      }else{
        digitalWrite(RED_LED_PIN, LOW); // выключаем светодиод что отвечает за температуру
        delay(50);
      }
    }else if(gpt<100){
      if(100-gpt>=5){
        digitalWrite(RED_LED_PIN, HIGH); // вкл светодиод что отвечает за температуру
        delay(50);
      }else{
        digitalWrite(RED_LED_PIN, LOW); // выключаем светодиод что отвечает за температуру
        delay(50);
      }
    }else{
      digitalWrite(RED_LED_PIN, LOW); // выключаем светодиод что отвечает за температуру
      delay(50);
    }
}

void deviationOfHum(){
    // deviation of hum from the set by 5%
    int gph = get_percent_hum();
    if(gph>100){
      if(gph-100>=5){
        digitalWrite(BLUE_LED_PIN, HIGH); // вкл светодиод что отвечает за hum
        delay(50);
      }else{
        digitalWrite(BLUE_LED_PIN, LOW); // выключаем светодиод что отвечает за hum
        delay(50);
      }
    }else if(gph<100){
      if(100-gph>=5){
        digitalWrite(BLUE_LED_PIN, HIGH); // вкл светодиод что отвечает за hum
        delay(50);
      }else{
        digitalWrite(BLUE_LED_PIN, LOW); // выключаем светодиод что отвечает за hum
        delay(50);
      }
    }else{
      digitalWrite(BLUE_LED_PIN, LOW); // выключаем светодиод что отвечает за hum
      delay(50);
    }
}

void bang_bang(int max_temp){
 if(temp<=max_temp) { // low 38/28 turn on bulb
    if(flag_relay){
       digitalWrite(RELAY_PIN, HIGH); //вкл лампочку
       flag_relay = false;
      }
    }
    else {
       digitalWrite(RELAY_PIN, LOW); //выкл лампочку
       flag_relay = true;
    }
}

int get_percent_temp(){
  if(flag_temp) return (temp*100)/TEMP_MAX_MAX;
  else return (temp*100)/TEMP_MIN_MAX;
}

int get_percent_hum(){
  return (hum*100)/HUM_NORM;
}

long microsecondsToInches(long microseconds){ // for sr04
  return microseconds / 74 / 2;
}

long microsecondsToCentimeters(long microseconds){ // for sr04
  return microseconds / 29 / 2;
}

void getDataToMoveInIncubator(){
  pinMode(HCSR_PING_PIN, OUTPUT);
  digitalWrite(HCSR_PING_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(HCSR_PING_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(HCSR_PING_PIN, LOW);
  
  pinMode(HCSR_ECHO_PIN, INPUT);
  duration = pulseIn(HCSR_ECHO_PIN, HIGH);
  inches = microsecondsToInches(duration);
  cm = microsecondsToCentimeters(duration);
  
  Serial.print(inches);
  Serial.print("in, ");
  Serial.print(cm);
  Serial.print("cm");
  Serial.println();
  
  delay(100);
}
