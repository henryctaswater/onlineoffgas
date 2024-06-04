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

//Definitions for enabling/disabling features (1 = enable, 0 = disable)
#define ENABLE_RTC 0
#define ENABLE_OXYGEN 0
#define ENABLE_CO2 0
#define ENABLE_SCREEN 0
#define ENABLE_MAF 0
#define ENABLE_SD 0


//LIBRARIES-------------------------------------------------------

//Sensor libraries
#include <Wire.h>
#include <splash.h>

#if ENABLE_CO2
#include <DFRobot_SCD4X.h>
#endif
#if ENABLE_OXYGEN
#include <DFRobot_OxygenSensor.h>
#endif

//OLED driver libraries
#if ENABLE_SCREEN
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#endif

//SD card libraries
#if ENABLE_SD
#include <SPI.h>
#include <SD.h>
#endif


//RTC
#if ENABLE_RTC
#include <RV-3028-C7.h>
RV3028 rtc;
#endif

int sec = 0;
int minute = 0;
int hour = 16;
int day = 1;
int date = 20;
int month = 5;
int year = 2024;

//Definitions-----------------------------------------------------------------------------------------------------

//MAF definitions
#if ENABLE_MAF
#define MAF_RANGE              150.0f    //Measurement Range
#define MAF_ZEROVOLTAGE        0.25f   //Zero Voltage 
#define MAF_FULLRANGEVOLTAGE   2.25f   //Full scale voltage
#define VREF               3.0f     //Reference voltage - SCOTT we need to double check this, every bit of documentation I read has a different referance voltage
//float MAFValuee          = 0; SCOTT - no need for this to be global variable
#endif

//CO2, temp, hum, pressure definitions
#if ENABLE_CO2
DFRobot_SCD4X SCD4X(&Wire, /*i2cAddr = */SCD4X_I2C_ADDR);
#endif

//O2Sensor definitions
#if ENABLE_OXYGEN
#define Oxygen_IICAddress ADDRESS_3 //Scott - ADDRESS_3 defined as 0x73 in DFRobot_OxygenSensor.h but online documentation says 0x77. we'll need to configure the switch on the oxygen board to A0=1, A1=1 to select address 3
#define OXYGEN_CONECTRATION 20.9  // The current concentration of oxygen in the air. Scott - what is the purpose of defining OXYGEN_CONECTRATION and OXYGEN_MV??
#define OXYGEN_MV           0     // The value marked on the sensor, Do not use must be assigned to 0.
DFRobot_OxygenSensor oxygen(&Wire);// ,Oxygen_IICAddress); //Scott - Added arguments for calling DFRobot_OxygenSensor constructor
#endif

//OLED definitions
#if ENABLE_SCREEN
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D ///< See datasheet for Address; 0x3C (ASW:OFF) or 0x3D (ASW:ON)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#endif

//Pin definitions definitions SCOTT - the "=" was missing here
const int MAFPin = A0;            // Mass airflow sensor analugue front end output 
//static const uint8_t D12 = 4; //ONLY FOR TESTING COMPILE ONLINE
const int ThreeWayPin = D12;      // Digital output the the relay contorillin the 3 way valve
const int chipSelect = D7;     // connect the the SDCS pin on the SD card reader

//Loop timers------------------------------------------------------------

/* timer intervals*/
//SCOTT: Temporarily lowered these values for initial testing
const long eventTime_sensor_read           = 1000;     //in ms         10 seconds      for updating the OLED display
const long eventTime_sensor_write          = 10000;    //in ms         5  minutes      for writing to the SD card

/* timer inital conditions*/
unsigned long previousTime_sensor_read     = 0;
unsigned long previousTime_sensor_write    = 0;

//Sensor config
uint8_t oxygenSensorSamplingSize = 10; //Num of samples that are averaged together when reading O2 values NOTE: I have no idea if 10 is a good number, need to play around and see what works well


//Struct representing a row of data using all available sensors
struct SensorValues//()
{
  // TODO: ADD TIMESTAMP TO THIS STRUCT
  //NOTE: A value of -1 represents no data this will be written to .csv as ""
  float oxygenConcentration = -1.0f;
  float co2Concentration = -1.0f;
  float ambientTemp = -1.0f;
  float relativeHumidity = -1.0f;
  float airPressure = -1.0f;
  float massAirFlow = -1.0f;

  //Get the name of a measured variable
  String GetMeasurementName(int i)
  {
    switch(i)
    {
      case 0: return "O2";
      case 1: return "CO2";
      case 2: return "Temp";
      case 3: return "RelativeHumidity";
      case 4: return "airPressure";
      case 5: return "AirFlow";
      default: return "ERROR";
    }
  }

  //Get the units of a measured variable
  String GetMeasurementUnits(int i)
  {
    switch(i)
    {
      case 0: return "%";
      case 1: return "ppm";
      case 2: return "degC";
      case 3: return "%";
      case 4: return "Pa";
      case 5: return "L/min";
      default: return "ERROR";
    }
  }

  //Get the value of a measured variable
  float GetMeasurementValue(int i)
  {
    switch(i)
    {
      case 0: return oxygenConcentration;
      case 1: return co2Concentration;
      case 2: return ambientTemp;
      case 3: return relativeHumidity;
      case 4: return airPressure;
      case 5: return massAirFlow;
      default: return -1.0f;
    }
  }

  //Get the string for the .csv header
  String getCSVHeader()
  {
    String headerString = "";
    for(int i=0; i<6; i++)
    {
      headerString += GetMeasurementName(i);
      headerString += "(";
      headerString += GetMeasurementUnits(i);
      headerString += ")";
      if(i<5)
      {headerString += ",";}
    }

    return headerString;
  }

  //Get a string formatted as a .csv row containing all measurements
  String getCSVrowString()
  {
    String row = "";

    for(int i=0; i<6; i++)
    {
      float value = GetMeasurementValue(i);

      if(value != -1.0f)
      {
        row += String(value);
      }
      else
      {
        row += "";
      }

      if(i<5)
      {row += ",";}
    }

    return row;
  }

  //Get a string containing all measurements suitable for printing to serial output
  String getPrintString()
  {
    String stringToPrint = "";

    for(int i=0; i<6; i++)
    {
      float value = GetMeasurementValue(i);

      stringToPrint += GetMeasurementName(i);
      stringToPrint +=" = ";
      if(value != -1)
      {
        stringToPrint += "N/A";
      }
      else 
      {
        stringToPrint += GetMeasurementUnits(i);
      }

      if(i<5)
      {stringToPrint += ",";}

    }

    return stringToPrint;
  }
};

//write a function that returns the time from the RTC, this is to simplify the RTC calls in the loop function. reserach this more
#if ENABLE_RTC
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
#endif

//a function to enable the easy reading off all sensors, the function ruturns a string of all sensor readings to the loop function
SensorValues ReadSensors()
{
  //String RTCcurrentTime = rtc.stringTimeStamp();
     
  //String dataString = String();

  SensorValues sensorReadings;

  //Add timestamp to string
  //dataString += RTCcurrentTime;

  //Read O2 data
  #if ENABLE_OXYGEN
  sensorReadings.oxygenConcentration = oxygen.getOxygenData(oxygenSensorSamplingSize);
  #endif

  //Read CO2 data
  #if ENABLE_CO2
  if(SCD4X.getDataReadyStatus()) 
  {
    DFRobot_SCD4X::sSensorMeasurement_t data;
    SCD4X.readMeasurement(&data);

    //Serial.print("Carbon dioxide concentration : ");
    //dataString += String(data.CO2ppm);
    //dataString +=(" ppm, ");
    sensorReadings.co2Concentration = data.CO2ppm;

    //Serial.print("Environment temperature : ");
    //dataString += String(data.temp);
    //dataString +=(" C, ");
    sensorReadings.ambientTemp = data.temp;

    //Serial.print("Relative humidity : ");
    //dataString += String(data.humidity);
    //dataString +=(" RH, ");
    sensorReadings.relativeHumidity = data.humidity;

    //Serial.print("Environment pressure : ");
    //dataString += String(data.pressure);
    //dataString +=(" Pa, ");
    //sensorReadings.airPressure = data.pressure;
  }
  #endif

  //Read MAF
  #if ENABLE_MAF
  int MAF_PinValue = analogRead(MAFPin);
  float MAF_PinVoltage = float(MAF_PinValue) / 4096.0f * VREF;
  float MAF_Value = MAF_RANGE*(MAF_PinVoltage - MAF_ZEROVOLTAGE)/(MAF_FULLRANGEVOLTAGE - MAF_ZEROVOLTAGE);
  sensorReadings.massAirFlow = MAF_Value;
  #endif

  //dataString += string(MAFValue);
  //dataString +=(" SLM, ");
          
  //Serial.print(dataString);  

  //return dataString;
  return sensorReadings;
}     

//A function to write a row of data to the log file , called from the main loop function.
#if ENABLE_SD
void DataLog(SensorValues measurementsToLog)
{
  File logFile;

  //Check if the file already exists.
  bool bNewFile = SD.exists("datalog.csv");

  //Open the file (or create it if it doesn't exist)
  logFile = SD.open("datalog.csv", FILE_WRITE);

  //Exit function if the file couldn't be opened/created correctly
  if(!logFile)
  {
    Serial.println("error opening datalog.csv");
    return;
  }

  //If the file is newly created, write the .csv header row
  if(bNewFile)
  {
    logFile.println(measurementsToLog.getCSVHeader());
  }

  //Write the sensor readings to the file
  logFile.println(measurementsToLog.getCSVrowString());

  Serial.print("Write to SD successful: datalog.csv file size = ");
  Serial.print(logfile.size());
  Serial.println(" bytes");

  //Close the file once done
  logFile.close();
}
#endif


/*-----------------------------------------------------------------SET-UP-------------------------------------------------------------------*/


void setup() 
{

  //Open serial port-----------------------------------------------------------
  Serial.begin(115200);
  Serial.println();

  //Initialise the wire library and join I2C bus as a controller
  Wire.begin(); //Scott - previously this was done further down the function but must instead be called at the start before any I2C communication

  //Configure pin mide for 3-way valve relay
  pinMode(ThreeWayPin, OUTPUT); //Scott - changed from ThreeWay to ThreeWayPin

  //Setup CO2, temp, hum, pressure sensor
  #if ENABLE_CO2
  while( !SCD4X.begin() )
  {
    Serial.println("Communication with CO2 sensor failed, please check connection");
    delay(1000);
  }
  Serial.println("I2c CO2 connect success!");
  #endif

  //Setup O2 Sensor
  #if ENABLE_OXYGEN
  while(!oxygen.begin(Oxygen_IICAddress))
  {
    Serial.println("Communication with O2 sensor failed, please check connection");
    delay(1000);
  }
  Serial.println("I2c O2 connect success!");
  #endif
    
  //Configure CO2 sensor
  #if ENABLE_CO2
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
  #endif

  //Setup RTC
  #if ENABLE_RTC
  while(!rtc.begin())
  {
    Serial.println("Communication with RTC failed, please check connection");
    delay(1000);
  }
  Serial.println("RTC connect success!");
  #endif

  //Setup SD Card--------------------------------------------------------------
  #if ENABLE_SD
  Serial.print("Initializing SD card...");
  // see if the card is present and can be initialized:
  while(!SD.begin(chipSelect)) 
  {
    Serial.println("SD Card failed, or not present");
    // don't do anything more:
  }
  Serial.println("SD card initialized.");
  #endif

  Serial.println("----Setup Complete----");
}


void loop() 
{
  unsigned long currentTime = millis();

  #if ENABLE_RTC
  RTCtime(); //Calls the RTC time function to set the time
  #endif

  //Eventually the millis value will overflow and reset to zero (after ~50d of data logging). When this happens we need to reset the value of previousTime_sensor_write
  if(currentTime < previousTime_sensor_write)
  {
    previousTime_sensor_write = 0;
    previousTime_sensor_read = 0;
  }

  //Reading the sensors and logging the data---------------------------------------------
  if( currentTime - previousTime_sensor_read >= eventTime_sensor_read) 
  {
    // SCOTT: changed how te SensorRead function works, better in this case to generate and return the sting in-function instead of passing in a blank string by-reference to the function
    // make a string for assembling the data to log:
    //String dataString = "";

    //Read the current value of all sensors
    SensorValues sensorData = ReadSensors();

    //Print the measured sensor values
    Serial.println(sensorData.getPrintString());

    if(currentTime - previousTime_sensor_write >= eventTime_sensor_write) 
    {
      //get the curret time to send to the datalog file
      #if ENABLE_SD
      DataLog(sensorData); //Calls the datalog function for recording data
      #endif

      //reset write timer
      previousTime_sensor_read = currentTime;
    }
   
   //update pervious sensor log time to current time
   previousTime_sensor_read = currentTime;
  }


  //need to add a callibration for the O2 sensor
  //should have a void SensorRead() function for reading all sensors so the same fucntion can be used to update the display as to update the datalogger

}
