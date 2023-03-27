/*****************************************************************
 *  This software is used to regulate the bolus heat/cooling system
 *  It consist of following hardware:
 *  1 pcs Arduino UNO
 *  1 pcs LCD Display 2 x 16 characters type "JHED162A or simular"
 *    using paralell interface
 *  1 pcs I2C to paralell converter
 *  2 pcs Digaital servo 25Kg metal gear, 4.8 - 6.8 V 
 *  1 pcs Flow rate sensor type "YF-S201"
 *  2 pcs temp sensors type Dallas DS18B20
 *  1 pcs switch
 *  1 pcs Power supply 12V 3A
 *  1 pcs DC/DC (12 -> 6.8V)
 * 
 ***************************************************************/

#define VERSION "2023-04-20"

#include <Arduino.h>

#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Servo.h>

float GetTemp(int);
float CalibrateTemperatur(int, float);
float Trim_temperature(float);
void SetBallValve(int, int);
int  GetCommand(void);
void AdjustTemperature(int);



#define SERIAL_SPEED 9600  // Speed serial port

#define ZERO 0
#define NONE -1      // No command from computer
#define MIN_TEMP 10  // 10 degrees Celsius
#define MAX_TEMP 50  // 50 degrees Celsius

// Power pins
#define POWER_RELAY 5 // Relay holding the system power
#define POWER_SWITCH  6 // Power switch signal

// Valve parameters
#define COLD_VALVE 0
#define HOT_VALVE 1
#define CLOSE_COLD_VALVE 105    
#define CLOSE_HOT_VALVE 105    // PWM values -> Closed 105 Max open 5

// Servo connections PWM pins
#define HOT_PIN  9        
#define COLD_PIN 10
// Setup servos
Servo HotServo;
Servo ColdServo;

// Setup a oneWire instance to communicate with any Temperature device
#define ONE_WIRE_BUS 2                        // Data wire is plugged into digital pin 2 on the Arduino using 4k7 pullup
#define TEMP_PRIMARY  0
#define TEMP_SECONDARY 1
// Calibration of sensors
#define TEMP_PRIMARY_RAW_HIGH 98.3            // Here is the calibration messurement boiling water
#define TEMP_PRIMARY_RAW_LOW 0.06             // Here is the calibration messurement ice water
#define TEMP_PRIMARY_REFERENCE_HIGH 99.9      // Constant
#define TEMP_PRIMARY_REFERENCE_LOW 0.0        // Constant
#define TEMP_PRIMARY_RAW_RANGE TEMP_PRIMARY_RAW_HIGH-TEMP_PRIMARY_RAW_LOW
#define TEMP_PRIMARY_REFERENCE_RANGE TEMP_PRIMARY_REFERENCE_HIGH-TEMP_PRIMARY_REFERENCE_LOW
#define TEMP_SECONDARY_RAW_HIGH 98.5          // Here is the calibration messurement boiling water
#define TEMP_SECONDARY_RAW_LOW 0.12           // Here is the calibration messurement ice water
#define TEMP_SECONDARY_REFERENCE_HIGH 99.9    // Constant
#define TEMP_SECONDARY_REFERENCE_LOW 0.0      // Constant
#define TEMP_SECONDARY_RAW_RANGE TEMP_SECONDARY_RAW_HIGH-TEMP_SECONDARY_RAW_LOW
#define TEMP_SECONDARY_REFERENCE_RANGE TEMP_SECONDARY_REFERENCE_HIGH-TEMP_SECONDARY_REFERENCE_LOW


//uint8_t secondary_temp_sensor[] = {0x28, 0xDC, 0x19, 0x27, 0x00, 0x00, 0x80, 0x52}; // Lab sensor
uint8_t primary_temp_sensor[] = {0x28, 0xED, 0x80, 0x7C, 0x20, 0x22, 0x09, 0x54};    // Primary temp sensor address
uint8_t secondary_temp_sensor[] = {0x28, 0xC2, 0x49, 0x7A, 0x20, 0x22, 0x09, 0x1E};  // Secondary temp sensor address

OneWire oneWire(ONE_WIRE_BUS);                // Make fysical connection
DallasTemperature sensors(&oneWire);          // Pass oneWire reference to DallasTemperature library

// Selection switch (Manual/computer temp setting)
#define SELECTION_SWITCH_PIN 3
bool ManMode = false;                         // Defalt Computer communication

// Potentiometer manual mode
#define POT_PIN A0

// Setup LCD, I2C address 0x27, 16 column and 2 rows
#define I2C_ADDRESS  0x27                     // I2C to parallel address
#define ROWS 2                                // 2 rows display
#define COLUMN 16                             // 16 character
LiquidCrystal_I2C lcd(I2C_ADDRESS, COLUMN, ROWS); // Setup display
String  Mystring = "";                        // Used in LCD display


bool StartUp = true;
bool ManModeFlag = false;

bool PowerOff = false;
unsigned long timer;
int SetTemperature, temp = NONE;
int OldCommand = ZERO;

void setup() {

  //Setup I/O-pins
  pinMode(SELECTION_SWITCH_PIN, INPUT_PULLUP);
  pinMode(POWER_SWITCH, INPUT_PULLUP);
  pinMode(POWER_RELAY, OUTPUT);

  digitalWrite(POWER_RELAY, HIGH);

  // Servo pins
  HotServo.attach(HOT_PIN);    
  ColdServo.attach(COLD_PIN);
   
  
  // Setup serial port
  Serial.begin(SERIAL_SPEED);

  // Init Dallas library
  sensors.begin();	
  



  
  // Initialize the LCD
  lcd.init();                   
  lcd.backlight();
  lcd.clear();                 // clear display
  lcd.setCursor(0, 0);         // move cursor to   (0, 0)
  lcd.print("Starting.....");  // Staring text

    // No input water yet 
  SetBallValve(COLD_VALVE, CLOSE_COLD_VALVE);
  SetBallValve(HOT_VALVE, CLOSE_HOT_VALVE);
  delay(3000);                  // Wait for valves to close

  if(digitalRead(SELECTION_SWITCH_PIN)){
    Mystring = "SetV (--)";
    Mystring.concat("  MAN");
  }else{
    Mystring = "SetV (--)";
    Mystring.concat("   COMP ");
  }
  lcd.setCursor(0, 0);
  lcd.print(Mystring); 
  Mystring = "Bolus "; 
  Mystring.concat(Trim_temperature(GetTemp(TEMP_SECONDARY)));
  Mystring.remove(10,1);
  lcd.setCursor(0, 1);
  lcd.print(Mystring); 

  
}     

void loop() {
  //************************* OBS!!!!!!!!!

  


  delay(100);


  if(!digitalRead(POWER_SWITCH)){
    if(PowerOff == false){
      PowerOff = true;
      timer = millis(); 
    }else{
      if(millis() - timer > 2000){
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Power Off ....."); 
        delay(4000);
        digitalWrite(POWER_RELAY, LOW); // Power off
      }
    }
  }

  ManMode = (bool)digitalRead(SELECTION_SWITCH_PIN);

  if(ManMode){    // Manuell mode?
    ManModeFlag = true;
    if(analogRead(POT_PIN) == ZERO){                // No temp is choosen, close valves
      SetBallValve(COLD_VALVE, CLOSE_COLD_VALVE);
      SetBallValve(HOT_VALVE, CLOSE_HOT_VALVE);
      Mystring = "SetV (--)";
      Mystring.concat("  MAN");
      lcd.setCursor(0, 0);
      lcd.print(Mystring); 
    }else{
      SetTemperature = map(analogRead(POT_PIN), 0, 1023, 10, 50);   // From pot min to max, map to 10 to 50 degrees  
      Mystring = "SetV (";
      Mystring.concat(SetTemperature);
      Mystring.concat(")  MAN  ");
      lcd.setCursor(0, 0);
      lcd.print(Mystring);
      AdjustTemperature(SetTemperature); 
    }
    Mystring = "Bolus "; 
    Mystring.concat(Trim_temperature(GetTemp(TEMP_SECONDARY)));
    Mystring.remove(10,1);
    lcd.setCursor(0, 1);
    lcd.print(Mystring); 
  }else{          // Check if computer have send a command
    temp = GetCommand();
    if(ManModeFlag){ // If manual mode before, go back to old computer value
      ManModeFlag = false;
      temp = OldCommand;  
    }
    if(temp != NONE){
     //Legal command is received
      SetTemperature = temp; 
      if(SetTemperature == ZERO){
        OldCommand = SetTemperature;
        SetBallValve(COLD_VALVE, CLOSE_COLD_VALVE);  // Close input water
        SetBallValve(HOT_VALVE, CLOSE_HOT_VALVE);
        Mystring = "SetV (--)";
        Mystring.concat("   COMP ");
        lcd.setCursor(0, 0);
        lcd.print(Mystring);
        Mystring = "Bolus "; 
        Mystring.concat(Trim_temperature(GetTemp(TEMP_SECONDARY)));
        Mystring.remove(10,1);
        lcd.setCursor(0, 1);
        lcd.print(Mystring); 
      }else if(SetTemperature >= MIN_TEMP && SetTemperature <= MAX_TEMP){
        OldCommand = SetTemperature;
        //lcd.clear(); 
        Mystring = "SetV (";
        Mystring.concat(SetTemperature);
        Mystring.concat(")   COMP ");
        lcd.setCursor(0, 0);
        lcd.print(Mystring);
        Mystring = "Bolus "; 
        Mystring.concat(Trim_temperature(GetTemp(TEMP_SECONDARY)));
        Mystring.remove(10,1);
        lcd.setCursor(0, 1);
        lcd.print(Mystring); 
      Serial.println(SetTemperature);
        AdjustTemperature(SetTemperature);
      } 
    }

  }

  

}

 /*******************************************************
* Read the temperatur sensor (water)
* Input:  Sensor number
* Return: Temperature in Celcius
*********************************************************/
float GetTemp(int sensor){

  float tempC = 0.0;

  sensors.requestTemperatures();
  if(sensor == TEMP_PRIMARY){
    tempC = sensors.getTempC(primary_temp_sensor);
  }else if(sensor == TEMP_SECONDARY){
    tempC = sensors.getTempC(secondary_temp_sensor);
  }
  return(tempC);
 }



/*******************************************************
* Trim temperature value to decimal 0 or 5
* if decimal is >= 5 integer will be incremented and
* decimal will be 0 else decimal will be 5.
* Input:  Sensor number, temperature raw value
* Return: Calibrated temp value
*********************************************************/
float CalibrateTemperatur(int sensor, float RawValue){
  
  float CorrectedValue;
  
  if(sensor == TEMP_PRIMARY){
    CorrectedValue = (((RawValue - TEMP_PRIMARY_RAW_LOW)* TEMP_PRIMARY_REFERENCE_RANGE)/TEMP_PRIMARY_RAW_RANGE)+TEMP_PRIMARY_REFERENCE_LOW;
  }else{  // default TEMP_SECONDARY 
    CorrectedValue = (((RawValue - TEMP_SECONDARY_RAW_LOW)* TEMP_SECONDARY_REFERENCE_RANGE)/TEMP_SECONDARY_RAW_RANGE)+TEMP_SECONDARY_REFERENCE_LOW;
  }
  return(CorrectedValue);
}

/*******************************************************
* Trim temperature value to decimal 0 or 5
* if decimal is >= 5 integer will be incremented and
* decimal will be 0 else decimal will be 5.
* Input:  Temperature
* Return: Trimmed temperature
*********************************************************/
float Trim_temperature(float temperature){
  
  int temp_int, integer, decimal;

  temp_int = (int)(temperature * 10.0);
  integer = temp_int / 10; 
  decimal = temp_int % 10;

  if(decimal > 5){
    integer++;
    decimal = 0;
  }else if(decimal <5){
    decimal = 0;
  }
  return((int)(integer * 10 + decimal)/10.0);
}

/*******************************************************
* Setts the ball valve to position
* Value could be xx - yy (closed to full opened)
* decimal will be 0 else decimal will be 5.
* Input:  Valve number, value
* Return: none
*********************************************************/
void SetBallValve(int valve, int value){

  if(valve == COLD_VALVE){
    ColdServo.write(value);
  }else{  // Default HOT_VALVE
    HotServo.write(value);   
  }

}

/*******************************************************
* Check for new temperatur command 
* Value could be 10 - 50 or -1 if no command is found
* Input:  none
* Return: Temp value or -1 
*********************************************************/

int GetCommand(void){
  int temp;

/*

    if(Serial.available()){
    String buffer = Serial.readString();
    buffer.trim();
    if(buffer.charAt(0) == '#'){
      buffer.remove(0,1);
      buffer.remove(2);
      temp = buffer.toInt();
      if(temp == ZERO || (temp >= MIN_TEMP && temp <= MAX_TEMP)){
        return(temp);
      }
    }
*/
  if(Serial.available()){
    String buffer = Serial.readString();
    buffer.trim();Serial.println(buffer);
    if(buffer.charAt(0) == '#'){
      buffer.remove(0,1); 
      temp = buffer.toInt();
      if(temp == ZERO || (temp >= MIN_TEMP && temp <= MAX_TEMP)){
        return(temp);
      }
    }
  }else{
     return(NONE);
  }
  
 
}


/*******************************************************
* Adjust the bolus wather temp
* Value could be 10 - 50 
* Input:  Tempersture set value
* Return: non
*********************************************************/

void AdjustTemperature(int SetValue){

}