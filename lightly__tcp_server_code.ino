#include <SimpleTimer.h>
#include <Stepper.h>
#include "rgb_lcd.h"
#include <Wire.h>

//The code is written assuming that the curtain is open for the first time, meaning it will only close first
//and then open, and so on...

//Variables you may need to set:
float lightThreshold = 20.0;        //Threshold  value which decides whether to open or close the curtain
float mq2Threshold = 45.0;          //Enter the smoke sensor threshold values from 0 to 100 as needed
long lightCounterThreshold = 60000;  //Minimum successive light readings before Lightly decides to open or close the curtains
long interval = 20000;              //The motor will run for at least this time (in milliseconds)
int repetitions = 3;                //Increasing this number increases the time the motor runs in each go
int stepsPerRevolution = 400;       //The steps per revolution for your motor
int motorSpeed = 60;                //Speed of the motor. 60 is a good value for the Grove Stepper Motor for me

//Do not change these variables if you don't know what you're doing
String input;
float temperature, mq2Percent, light;   //These hold the sensor values
long previousMillis;                    //Holds the previous reading for the motor running timer
long openCounter, closeCounter;         //Counter telling how many successive readings taken to open or close curtains
bool motorMoving = false;               //Sets the motor as not moving in the start of the program
bool curtainState = false;              //curtainState false means curtain is open and true means closed
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
  delay(2000);
}

void loop()
{
  getSensorData();
  timer.run();
  openWithLight();
  checkForChangingVariables();
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
  Serial1.println();
  Serial1.print("Temperature: ");
  Serial1.print(temperature);
  Serial1.println(" C");
  Serial1.print("Smoke: ");
  Serial1.print(mq2Percent);
  Serial1.println("%");
  Serial1.print("Light: ");
  Serial1.println(light);
  Serial1.print("Curtain State: ");
  if (curtainState == true) {
    Serial1.println("Opened");
  }
  if (curtainState == false) {
    Serial1.println("Closed");
  }
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

void checkForChangingVariables() {
  input = Serial1.readString();
  if (input == "lightThreshold") {
    Serial1.println();
    Serial1.print("'lightThreshold' value is: ");
    Serial1.println(lightThreshold);
    Serial1.print("Please enter the new value from 1 to 100: ");
    lightThreshold = 0.0;
    while (lightThreshold == 0.0) {
      lightThreshold = Serial1.parseFloat();
    }
    Serial1.println();
    Serial1.print("New 'lightThreshold' value is: ");
    Serial1.println(lightThreshold);
    Serial1.println();
  }
  if (input == "mq2Threshold") {
    Serial1.println();
    Serial1.print("'mq2Threshold' value is: ");
    Serial1.println(mq2Threshold);
    Serial1.print("Please enter the new value from 1 to 100: ");
    mq2Threshold = 0.0;
    while (mq2Threshold == 0.0) {
      mq2Threshold = Serial1.parseFloat();
    }
    Serial1.println();
    Serial1.print("New 'mq2Threshold' value is: ");
    Serial1.println(mq2Threshold);
    Serial1.println();
  }
  if (input == "lightCounterThreshold") {
    Serial1.println();
    Serial1.print("'lightCounterThreshold' value is: ");
    Serial1.println(lightCounterThreshold);
    Serial1.print("Please enter the new value: ");
    lightCounterThreshold = 0;
    while (lightCounterThreshold == 0) {
      lightCounterThreshold = Serial1.parseInt();
    }
    Serial1.println();
    Serial1.print("New 'lightCounterThreshold' value is: ");
    Serial1.println(lightCounterThreshold);
    Serial1.println();
  }
  if (input == "interval") {
    Serial1.println();
    Serial1.print("'interval' value is: ");
    Serial1.println(interval);
    Serial1.print("Please enter the new value: ");
    interval = 0;
    while (interval == 0) {
      interval = Serial1.parseInt();
    }
    Serial1.println();
    Serial1.print("New 'interval' value is: ");
    Serial1.println(interval);
    Serial1.println();
  }
  if (input == "repetitions") {
    Serial1.println();
    Serial1.print("'repetitions' value is: ");
    Serial1.println(repetitions);
    Serial1.print("Please enter the new value (at least 1): ");
    repetitions = 0;
    while (repetitions == 0) {
      repetitions = Serial1.parseInt();
    }
    Serial1.println();
    Serial1.print("New 'repetitions' value is: ");
    Serial1.println(repetitions);
    Serial1.println();
  }
  if (input == "stepsPerRevolution") {
    Serial1.println();
    Serial1.print("'stepsPerRevolution' value is: ");
    Serial1.println(stepsPerRevolution);
    Serial1.print("Please enter the new value: ");
    stepsPerRevolution = 0;
    while (stepsPerRevolution == 0) {
      stepsPerRevolution = Serial1.parseInt();
    }
    Serial1.println();
    Serial1.print("New 'stepsPerRevolution' value is: ");
    Serial1.println(stepsPerRevolution);
    Serial1.println();
  }
  if (input == "motorSpeed") {
    Serial1.println();
    Serial1.print("'motorSpeed' value is: ");
    Serial1.println(motorSpeed);
    Serial1.print("Please enter the new value: ");
    motorSpeed = 0;
    while (motorSpeed == 0) {
      motorSpeed = Serial1.parseInt();
    }
    Serial1.println();
    Serial1.print("New 'motorSpeed' value is: ");
    Serial1.println(motorSpeed);
    Serial1.println();
  }
}