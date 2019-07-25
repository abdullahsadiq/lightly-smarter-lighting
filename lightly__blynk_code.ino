#include <Wire.h>
#include <Stepper.h>
#include "rgb_lcd.h"
#include <Ethernet.h>
#include <SimpleTimer.h>
#include <BlynkSimpleStream.h>

//Defining Blynk virtual pins
#define vTEMPERATURE_PIN V0
#define vMQ2_PIN V1
#define vLIGHT_PIN V2
#define vEVENTOR_PIN V3
#define vZERGBA_PIN V4
#define vSWITCH_PIN V5
#define vCURTAINSTATE_PIN V6

//The code is written assuming that the curtain is open for the first time, meaning it will only close first
//and then open, and so on...

//Variables you may need to set:
char auth[] = "YourAuthToken";      //You should get this from the Blynk app
float lightThreshold = 20.0;        //Threshold  value which decides whether to open or close the curtain
float mq2Threshold = 45.0;          //Enter the smoke sensor threshold values from 0 to 100 as needed
long lightCounterThreshold = 60000;  //Minimum successive light readings before Lightly decides to open or close the curtains
long interval = 20000;              //The motor will run for at least this time (in milliseconds)
int repetitions = 3;                //Increasing this number increases the time the motor runs in each go
int stepsPerRevolution = 400;       //The steps per revolution for your motor
int motorSpeed = 60;                //Speed of the motor. 60 is a good value for the Grove Stepper Motor for me

//Do not change these variables if you don't know what you're doing
float temperature, mq2Percent, light;   //These hold the sensor values
long previousMillis;                    //Holds the previous reading for the motor running timer
long openCounter, closeCounter;         //Counter telling how many successive readings taken to open or close curtains
bool motorMoving = false;               //Sets the motor as not moving in the start of the program
bool curtainState = false;              //curtainState false means curtain is open and true means closed
int eventorValue, switchValue;          //Variables for the holding widget values
int pinLight = A0;                      //Pin for the light module
int pinTemp = A1;                       //Pin for the temperature module
int pinMq2 = A2;                        //Pin for the gas sensor
int colorR = 255, colorG = 255, colorB = 255;    //Default colour is white for the Grove RGB LCD

//Initializing the LCD and Simpletimer library
rgb_lcd lcd;
SimpleTimer timer;

//Initialize the stepper library on pins 8 to 11
//Try 8,10,9,11 if everything appears ok and you still have issues with the motor. The stepper motor I used wouldn't
//work with the order 8,9,10,11. Browsing the internet about this issue showed that the wires can sometimes not be
//in the expected order. You can read about that here: https://forum.arduino.cc/index.php?topic=143276.0
Stepper myStepper(stepsPerRevolution, 8, 10, 9, 11);

void setup()
{
  Serial.begin(9600);
  Serial1.begin(115200);

  delay(1000); // Give the WIZ750SR a second to initialize

  Blynk.begin(Serial1, auth);

  //Setup the timed fuctions
  timer.setInterval(1000L, sendSensorValues);
  timer.setInterval(2000L, displayData);

  lcd.begin(16, 2);

  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);

  lcd.setRGB(colorR, colorG, colorB);
  lcd.setCursor(0, 0);
  lcd.print("    Lightly");
  lcd.setCursor(0, 1);
  lcd.print("Smarter Lighting");

  //Default curtain state is open
  Blynk.virtualWrite(vCURTAINSTATE_PIN, curtainState);
}

void loop()
{
  Blynk.run();
  getSensorData();
  timer.run();
  if (switchValue == 0) {
    openWithLight();
  }
  if (switchValue == 1) {
    openWithEventor();
  }
}

void getSensorData() {
  int B = 3975;
  float resistance;
  int mq2Value = analogRead(pinMq2);
  int val = analogRead(pinTemp);
  int sensorReading = analogRead(pinLight);
  light = map(sensorReading, 0, 1023, 0, 100);
  mq2Percent = (mq2Value / 1023.0) * 100.0;
  resistance = (float)(1023 - val) * 10000 / val;
  temperature = 1 / (log(resistance / 10000) / B + 1 / 298.15) - 273.15;
}

void displayData() {
  if (checkForSmoke() == true) {
    int colorRed = 255;
    int colorGr = 0;
    int colorBl = 0;
    lcd.clear();
    lcd.setRGB(colorRed, colorGr, colorBl);
    lcd.setCursor(0, 0);
    lcd.print("Fire detected!");
    lcd.setCursor(0, 1);
    lcd.print("MQ2: ");
    lcd.print(mq2Percent);
    lcd.print(" %");
  }
  else if (checkForSmoke() == false) {
    lcd.setRGB(colorR, colorG, colorB);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temperature);
    lcd.print("C");
    lcd.setCursor(0, 1);
    lcd.print("Light: ");
    lcd.print(light);
    lcd.print("%");
  }
}

void displayOpeningCurtains() {
  const int colorR = 255;
  const int colorG = 255;
  const int colorB = 0;
  lcd.setRGB(colorR, colorG, colorB);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("    Opening");
  lcd.setCursor(0, 1);
  lcd.print("   Curtains!");
}

void displayClosingCurtains() {
  const int colorR = 255;
  const int colorG = 140;
  const int colorB = 0;
  lcd.setRGB(colorR, colorG, colorB);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("    Closing");
  lcd.setCursor(0, 1);
  lcd.print("   Curtains!");
}

void sendSensorValues() {
  //Sending the sensor values to the Blynk server
  Blynk.virtualWrite(vCURTAINSTATE_PIN, curtainState);
  Blynk.virtualWrite(vTEMPERATURE_PIN, temperature);
  Blynk.virtualWrite(vMQ2_PIN, mq2Percent);
  Blynk.virtualWrite(vLIGHT_PIN, light);
}

bool checkForSmoke() {
  if (mq2Percent > mq2Threshold) {
    return true;
  } else   return false;
}

void motorOpenOrClose() {
  if ((motorMoving == true) && (curtainState == true) && (interval > (millis() - previousMillis))) {
    displayClosingCurtains();
    for (int i = 0; i <= repetitions; i++) {
      myStepper.setSpeed(motorSpeed);
      myStepper.step(-stepsPerRevolution);
    }
  }
  if ((motorMoving == true) && (curtainState == false) && (interval > (millis() - previousMillis))) {
    displayOpeningCurtains();
    for (int i = 0; i <= repetitions; i++) {
      myStepper.setSpeed(motorSpeed);
      myStepper.step(stepsPerRevolution);
    }
  }
}

void openWithEventor() {
  if (curtainState == 0) {
    if ((eventorValue == 0) && (motorMoving == false)) {
      previousMillis = millis();
      motorMoving = true;
      curtainState = true;
    }
  }
  if (curtainState == 1) {
    if ((eventorValue == 1) && (motorMoving == false)) {
      previousMillis = millis();
      motorMoving = true;
      curtainState = false;
    }
  }
  motorOpenOrClose();
  if ((motorMoving == true) && ((millis() - previousMillis) > interval)) {
    myStepper.setSpeed(0);
    myStepper.step(stepsPerRevolution);
    motorMoving = false;
    eventorValue = 2; // Just a random number to wait for the next command. Not exactly needed!
    curtainState != curtainState;
  }
}

void openWithLight() {
  if (light >= lightThreshold) {
    if (curtainState == 1) {
      openCounter++;
    }
  }
  else {
    openCounter = 0;
  }
  if (light < lightThreshold) {
    if (curtainState == 0) {
      closeCounter++;
    }
  }
  else {
    closeCounter = 0;
  }
  if ((closeCounter > lightCounterThreshold) && (motorMoving == false)) {
    previousMillis = millis();
    motorMoving = true;
    curtainState = true;
  }
  if ((openCounter > lightCounterThreshold) && (motorMoving == false)) {
    previousMillis = millis();
    motorMoving = true;
    curtainState = false;
  }
  motorOpenOrClose();
  if ((motorMoving == true) && ((millis() - previousMillis) > interval)) {
    myStepper.setSpeed(0);
    myStepper.step(stepsPerRevolution);
    motorMoving = false;
    curtainState != curtainState;
    openCounter = 0;
    closeCounter = 0;
  }
}

BLYNK_WRITE(vZERGBA_PIN) {
  colorR = param[0].asInt();
  colorG = param[1].asInt();
  colorB = param[2].asInt();
  lcd.setRGB(colorR, colorG, colorB);
}

BLYNK_WRITE(vEVENTOR_PIN) {
  //Use the Eventor widget to check if it's time to do a task
  eventorValue =  param.asInt(); // 0 to 1
}

BLYNK_WRITE(vSWITCH_PIN) {
  //Default is Light control
  switchValue =  param.asInt(); // 0 to 1
}