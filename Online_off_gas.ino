/*
Title:     Online off-gas data logger
Version:   V0.1
Author:    Henry Coleman
Email:     henry.coleman@taswater.com.au
CM Ref:    #########


Version History:
    V0.1 - initial draft - Hnery Coleman


Description:

This code has been developed for the perpouse of online off-gas testing using low cost arduino compatible components.
The off-gas data is read from the sensors and written to an SD card

There is an intermittent O2 sensor callibration scedule to recalibrate the O2 sensor back to the refernece atmostphere concentration of 20.9%

S

*/

//start of code
//----------------------------------------------------------------DEFINITIONS------------------------------------------------------------------------------

//LIBRARIES-------------------------------------------------------

//Sensor libraries
#include <Wire.h>
#include <RV3028.h>
#include <Adafruit_SSD1306.h>
#include <splash.h>
#include <DFRobot_SCD4X.h>
#include <DFRobot_OxygenSensor.h>

//SD card libraries
#include <SPI.h>
#include <SD.h>

//Pin definitions definitions
#define MAF A0            // Mass airflow sensor analugue front end output 
#define 3way D12          // Digital output the the relay contorillin the 3 way valve
#define ChipSelect D7     // connect the the SDCS pin on the SD card reader

//Loop timers-----------------------------------------------------

/* timer intervals*/
const long eventTime_sensor_read           = 1000;     //in ms         10 seconds      for updating the OLED display
const long eventTime_sensor_write          = 300000;   //in ms         5  minutes      for writing to the SD card

/* timer inital conditions*/
unsigned long previousTime_sensor_read     = 0;
unsigned long previousTime_sensor_write    = 0;


void setup() {
  //Open serial port
  Serial.begin(115200);
  Serial.println();
  
}

void loop() {
    
}
