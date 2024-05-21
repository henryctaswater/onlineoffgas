/*
Title:        Online off-gas data logger
Version:      V0.1
Last Update:  20/05/2-24
Author:       Henry Coleman
Email:        henry.coleman@taswater.com.au
CM Ref:       #########


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
#include <splash.h>
#include <DFRobot_SCD4X.h>
#include <DFRobot_OxygenSensor.h>

//OLED driver libraries
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

//SD card libraries
#include <SPI.h>
#include <SD.h>


//RTC
#include <RV-3028-C7.h>
RV3028 rtc;

int sec = 0;
int minute = 0;
int hour = 16;
int day = 1;
int date = 20;
int month = 5;
int year = 2024;

//Definitions-----------------------------------------------------------------------------------------------------

//MAF definitions
#define RANGE              150    //Measurement Range
#define ZEROVOLTAGE        0.25   //Zero Voltage 
#define FULLRANGEVOLTAGE   2.25   //Full scale voltage
#define VREF               3      //Reference voltage
float MAFValuee          = 0;

//CO2, temp, hum, pressure definitions
DFRobot_SCD4X SCD4X(&Wire, /*i2cAddr = */SCD4X_I2C_ADDR);

//O2Sensor definitions
#define Oxygen_IICAddress ADDRESS_3
#define OXYGEN_CONECTRATION 20.9  // The current concentration of oxygen in the air.
#define OXYGEN_MV           0     // The value marked on the sensor, Do not use must be assigned to 0.
DFRobot_OxygenSensor oxygen;

//OLED definitions
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D ///< See datasheet for Address; 0x3C (ASW:OFF) or 0x3D (ASW:ON)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Pin definitions definitions
const int MAFPin A0;            // Mass airflow sensor analugue front end output 
const int ThreeWayPin D12;      // Digital output the the relay contorillin the 3 way valve
const int ChipSelect D7;     // connect the the SDCS pin on the SD card reader

//Loop timers------------------------------------------------------------

/* timer intervals*/
const long eventTime_sensor_read           = 1000;     //in ms         10 seconds      for updating the OLED display
const long eventTime_sensor_write          = 300000;   //in ms         5  minutes      for writing to the SD card

/* timer inital conditions*/
unsigned long previousTime_sensor_read     = 0;
unsigned long previousTime_sensor_write    = 0;


//write a function that returns the time from the RTC, this is to simplify the RTC calls in the loop function. reserach this more
void RTCtime () {
  //Set RTC from Arduino compiler
    //PRINT TIME
  if (rtc.updateTime() == false) //Updates the time variables from RTC
  {
    Serial.print("RTC failed to update");
  } else {
    String currentTime = rtc.stringTimeStamp();
    Serial.println(currentTime + "     \'s\' = set time     \'1\' = 12 hours format     \'2\' = 24 hours format");
  }
  //SET TIME?
  if (Serial.available()) {
    switch (Serial.read()) {
      case 's':
        //Use the time from the Arduino compiler (build time) to set the RTC
        //Keep in mind that Arduino does not get the new compiler time every time it compiles. to ensure the proper time is loaded, open up a fresh version of the IDE and load the sketch.
        if (rtc.setToCompilerTime() == false) {
          Serial.println("Something went wrong setting the time");
        }
        //Uncomment the below code to set the RTC to your own time
        /*if (rtc.setTime(sec, minute, hour, day, date, month, year) == false) {
          Serial.println("Something went wrong setting the time");
          }*/
        break;
      case '1':
        rtc.set12Hour();
        break;

      case '2':
        rtc.set24Hour();
        break;
    }
  }
}

//a function to enable the easy reading off all sensors, the function ruturns a string of all sensor readings to the loop function
void SensorRead(dataString)
      RTCcurrentTime = rtc.stringTimeStamp();
      dataString += RTCcurrentTime;

      //Read O2 data
      float oxygenData = oxygen.getOxygenData(COLLECT_NUMBER);
        dataString += String(oxygenData);
        dataString += "%,";
        
      Serial.print(dataString);
      
      //Read CO2 data
      if(SCD4X.getDataReadyStatus()) {

          DFRobot_SCD4X::sSensorMeasurement_t data;
          SCD4X.readMeasurement(&data);

          //Serial.print("Carbon dioxide concentration : ");
          dataString += String(data.CO2ppm);
          dataString +=(" ppm, ");

          //Serial.print("Environment temperature : ");
          dataString += String(data.temp);
          dataString +=(" C, ");

          //Serial.print("Relative humidity : ");
          dataString += String(data.humidity);
          dataString +=(" RH, ");

          //Serial.print("Environment pressure : ");
          dataString += String(data.pressure);
          dataString +=(" Pa, ");
        
        Serial.print(dataString);

      //Read MAF
          MAFValue = analogRead(MAFPin)*VREF;
          MAFValue = MAFValue / 1024;
          MAFValue = RANGE*(MAFValue - ZEROVOLTAGE)/(FULLRANGEVOLTAGE - ZEROVOLTAGE);
          dataString += string(MAFValue);
          dataString +=(" SLM, ");
          
        Serial.print(dataString);  

        return dataString
        
//A function to append the data string to the log file , called from the main loop function.
void DataLog(dataString){

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("datalog.csv", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.csv");

}
}



/*-----------------------------------------------------------------SET-UP-------------------------------------------------------------------*/


void setup() {


  //Open serial port-----------------------------------------------------------
  Serial.begin(115200);
  Serial.println();

  //Configure pin mide for 3-way valve relay
  pinMode(ThreeWay, OUTPUT);

  //Setup CO2, temp, hum, pressure sensor
    while( !SCD4X.begin() ){
      Serial.println("Communication with CO2 sensor failed, please check connection");
      delay(1000);
    }
    Serial.println("I2c CO2 connect success!");

  //Setup O2 Sensor
    while(!oxygen.begin(Oxygen_IICAddress)){
      Serial.println("Communication with O2 sensor failed, please check connection");
      delay(1000);
    }
    Serial.println("I2c O2 connect success!");
    
    
    SCD4X.enablePeriodMeasure(SCD4X_STOP_PERIODIC_MEASURE);

    SCD4X.setTempComp(4.0);

    float temp = 0;
    temp = SCD4X.getTempComp();
    Serial.print("The current temperature compensation value : ");
    Serial.print(temp);
    Serial.println(" C");

    SCD4X.setSensorAltitude(10);
    uint16_t altitude = 0;
    altitude = SCD4X.getSensorAltitude();
    Serial.print("Set the current environment altitude : ");
    Serial.print(altitude);
    Serial.println(" m");
  
    SCD4X.enablePeriodMeasure(SCD4X_START_PERIODIC_MEASURE);
    Serial.println();

 //Setup RTC
  Wire.begin();
    while(!rtc.begin()){
      Serial.println("Communication with RTC failed, please check connection");
      delay(1000);
    }
    Serial.println("RTC connect success!");


  //Setup SD Card--------------------------------------------------------------
    Serial.print("Initializing SD card...");
  // see if the card is present and can be initialized:
   while(!SD.begin(chipSelect)) {
    Serial.println("SD Card failed, or not present");
    // don't do anything more:
    }
    Serial.println("SD card initialized.");
}


void loop() {
  unsigned long CurrentTime = millis();

   RTCtime() //Calls the RTC time function to set the time

  //Reading the sensors and logging the data---------------------------------------------
  if( currentTime - previousTime_sensor_write >= eventTime_sensor_write) {
      // make a string for assembling the data to log:
      String dataString = "";

      SensorRead(dataString)

      //get the curret time to send to the datalog file
      DataLog(String dataString, String RTCcurrentTime) //Calls the datalog function for recording data
      Serial.println();
   
   //update pervious sensor log time to current time
   previousTime_sensor_write = currentTime;
  }

//need to add a callibration for the O2 sensor
//should have a void SensorRead() function for reading all sensors so the same fucntion can be used to update the display as to update the datalogger

}
