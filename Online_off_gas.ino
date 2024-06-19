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
#define ENABLE_RTC 1
#define ENABLE_OXYGEN 1
#define ENABLE_CO2 1
#define ENABLE_SCREEN 1
#define ENABLE_MAF 1
#define ENABLE_SD 1


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

//ESP32 Preferences Library
#include <Preferences.h>

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

//Time variables
byte time_year1 = 2;
byte time_year2 = 0;
byte time_year3 = 2;
byte time_year4 = 4;
byte time_month1 = 0;
byte time_month2 = 6;
byte time_day1= 1;
byte time_day2= 4;
byte time_hr1= 1;
byte time_hr2= 6;
byte time_min1= 5;
byte time_min2= 2;
byte time_sec1= 2;
byte time_sec2= 1;

//Preferenes storage space (ESP32)
Preferences preferences;

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

//Setup program and display enums
enum programStateEnum
{
  StartUp,
  Paused,
  Monitoring,
  Calibrating
};

enum displayStateEnum
{
  currentProgramTask,
  SetTime,
  ManualCalibration,
  Error
};

//enum buttonCommandEnum
//{
//  None,
//  NextDisplayScreen,
//  StartMonitoring_ContinueFromLast,
//  StartMonitoring_NewFile,
//  StopMonitoring,
// DoManualCalibration,
//  AdjustTime_Up,
// AdjustTime_NextField
//};


//enum errorCodesEnum
//{
//  error_O2Failed,
//  error_CO2Failed,
//  error_SDFailed,
//  error_RTCFailed,
//};

//State variables
bool startupComplete = false;
int startupStep = 0;
programStateEnum progamState = StartUp;
displayStateEnum displayState = currentProgramTask;
unsigned int taskDelay = 0;
unsigned long millis_lastLoop = 0;

//Input variables DO WE NEED TO CONFIGURE THE INPUT PIN FOR THE BUILD IN BUTTON?
bool lastButtonPinReading = false; //The button reading from the last loop
bool debouncedButtonState = false; //The last 
unsigned long lastButtonPinChangeTime = 0; //The last time (ms) that the button pin changed
unsigned long debounceDelay = 20; //ms

byte buttonMultiPressCounter = 0; //Num times button has been pressed 
unsigned long lastButtonPressStartTime = 0; //Time stamp of start of last button press
short buttonMultiPressMaxDuration = 400; 

byte buttonMultiPressEventBuffer = 0;

byte displaySelectedOption = 0;

String logFileName;
bool previousLogFileExists = false;



//Display variables
#define DISPLAY_REFRESH_RATE_MS 100
unsigned long lastDisplayRefreshTime = 0;


//Struct representing a row of data using all available sensors
struct SensorValues//()
{
  //NOTE: A value of -1 represents no data this will be written to .csv as ""
  String timeStamp = "1900/1/1 00:00:00";
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
    headerString += "Timestamp(UTC+10:00),";
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

    row += timeStamp;
    row += ",";

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

    stringToPrint += timeStamp;
    stringToPrint += " -  ";

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

SensorValues lastReading;

//write a function that returns the time from the RTC, this is to simplify the RTC calls in the loop function. reserach this more
#if ENABLE_RTC
void RTCtime () 
{
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

#if ENABLE_RTC
String GetCurrentTime() 
{
  if(rtc.updateTime() == false)
  {
    Serial.print("RTC failed to update");
  }
  
  return rtc.stringTimeStamp();
}
#endif

//a function to enable the easy reading off all sensors, the function ruturns a string of all sensor readings to the loop function
SensorValues ReadSensors()
{
  //String RTCcurrentTime = rtc.stringTimeStamp();
     
  //String dataString = String();

  SensorValues sensorReadings;

  //Get timestamp
  sensorReadings.timeStamp = GetCurrentTime();

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
  bool bNewFile = SD.exists(logFileName);

  //Open the file (or create it if it doesn't exist)
  logFile = SD.open(logFileName, FILE_WRITE);

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
  Serial.print(logFile.size());
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

  //Initialise the preferences library. This is used for flash storage on ESP32
  while(!preferences.begin("my_variables", false))
  {
    Serial.println("Initialising preferences");
    delay(500);
  }

  //Setup CO2, temp, hum, pressure sensor
  //#if ENABLE_CO2
  //while( !SCD4X.begin() )
  //{
  //  Serial.println("Communication with CO2 sensor failed, please check connection");
  //  delay(1000);
  //}
  //Serial.println("I2c CO2 connect success!");
  //#endif

  //Setup O2 Sensor
  //#if ENABLE_OXYGEN
  //while(!oxygen.begin(Oxygen_IICAddress))
  //{
  //  Serial.println("Communication with O2 sensor failed, please check connection");
  //  delay(1000);
  //}
  //Serial.println("I2c O2 connect success!");
  //#endif
    
  //Configure CO2 sensor
  //#if ENABLE_CO2
  //SCD4X.enablePeriodMeasure(SCD4X_STOP_PERIODIC_MEASURE);

  //SCD4X.setTempComp(4.0);

  //float temp = 0;
  //temp = SCD4X.getTempComp();
  //Serial.print("The current temperature compensation value : ");
  //Serial.print(temp);
  //Serial.println(" C");

  //SCD4X.setSensorAltitude(10);
  //uint16_t altitude = 0;
  //altitude = SCD4X.getSensorAltitude();
  //Serial.print("Set the current environment altitude : ");
  //Serial.print(altitude);
  //Serial.println(" m");
  //neeed to reverse engineer the library to convert altitude to pressure
  
  //SCD4X.enablePeriodMeasure(SCD4X_START_PERIODIC_MEASURE);
  //Serial.println();
  //#endif

  //Setup RTC
  //#if ENABLE_RTC
  //while(!rtc.begin())
  //{
  //  Serial.println("Communication with RTC failed, please check connection");
  //  delay(1000);
  //}
  //rtc.set24Hour(); //Set the rtc to 24h time
  //Serial.println("RTC connect success!");
  //#endif

  //Setup SD Card--------------------------------------------------------------
  //#if ENABLE_SD
  //Serial.print("Initializing SD card...");
  //// see if the card is present and can be initialized:
  //while(!SD.begin(chipSelect)) 
  //{
  //  Serial.println("SD Card failed, or not present");
  //  // don't do anything more:
  //}
  //Serial.println("SD card initialized.");
  //#endif
  //Serial.println("----Setup Complete----");
}


void loop() 
{
  unsigned long currentTime = millis();
  unsigned int timestep;
  if(currentTime < millis_lastLoop)
  {
    timestep = 0;
  }
  else
  {
    timestep = currentTime - millis_lastLoop;
  }

  millis_lastLoop = currentTime;

  if(taskDelay > 0)
  {
    if(timestep>taskDelay)
    {
      taskDelay = 0;
    }
    else
    {
      taskDelay = taskDelay - timestep;
    }
  }

  //Button de-bouncing and detect number of clicks
  bool currentButtonReading = digitalRead(D4)==LOW;//D4 is the general purpose button on the ESP32 board
  bool isButtonPressStart = false;
  if(currentButtonReading != lastButtonPinReading)
  {
    lastButtonPinChangeTime = millis();
    lastButtonPinReading = currentButtonReading;
  }

  if((millis() - lastButtonPinChangeTime) > debounceDelay)
  {
    if(currentButtonReading != debouncedButtonState)
    {
      debouncedButtonState = currentButtonReading;
      if(debouncedButtonState == true)
      {
        isButtonPressStart = true;
        //lastButtonHoldPressTime = millis();
      }
    }

    //If button is held then manually register a single button press
    if((millis() - lastButtonPinChangeTime) > 200 && currentButtonReading == true)
    {
      lastButtonPinChangeTime = millis();
      buttonMultiPressEventBuffer = 1;
      buttonMultiPressCounter=0;
    }
  }

  if(isButtonPressStart)
  {
    buttonMultiPressCounter++;
    lastButtonPressStartTime = millis();
  }
  if(buttonMultiPressCounter>0 && (millis() - lastButtonPressStartTime) > buttonMultiPressMaxDuration)
  {
    buttonMultiPressEventBuffer = buttonMultiPressCounter;
    buttonMultiPressCounter = 0;
  }

  if(startupComplete)
  {
    detectAndProcessInput();
  }

  if(taskDelay == 0)
  {
    switch(progamState)
    {
      case StartUp:
      // Check that peripherals are connected and can communicate, configure sensors, etc
        taskLoop_Startup();
        break;

      case Paused:
      // Monitoring has either not started yet or has been paused

        break;


      case Monitoring:
      // Monitoring is being done periodically
        taskLoop_Monitoring();
        break;

      case Calibrating:
      // Calibration step, can be triggered manually or automatically

        break;

    }
  }



  //#if ENABLE_RTC
  //RTCtime(); //Calls the RTC time function to set the time
  //#endif


  if(millis() - lastDisplayRefreshTime > DISPLAY_REFRESH_RATE_MS)
  {
    refreshDisplay();
    lastDisplayRefreshTime = millis();
  }

    
  




}

void refreshDisplay()
{
  Serial.println(displayState);
  Serial.println(displaySelectedOption);
  display.clearDisplay();

  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);

  int selectionBox_X = 0;
  int selectionBox_Y = 0;
  int selectionBox_Width = 0;
  int selectionBox_Height = 0;
  String selectionText = "";


    switch(displayState)
    {
      case currentProgramTask:

        switch(progamState)
        {
          case StartUp:
             display.setCursor(0, 0);
             display.println(F("Start-Up"));
             display.setCursor(0, 16);
             switch(startupStep)
             {
                case 0: display.println("Connecting to CO2"); break;
                case 1: display.println("Configuring CO2"); break;
                case 2: display.println("Connecting to O2"); break;
                case 3: display.println("Setting up RTC"); break;
                case 4: display.println("Setting up SD Card"); break;
                case 5: display.println("Startup complete!!"); break;
             }

            break;

          case Paused:
          {
            
            String line1 = "   Ready To Start";
            String line2 = "Resume Last Log File";
            String line3 = preferences.getString("lastLogFile", "oh-no!");
            String line4 = "Start New File";

            display.setCursor(0, 0);
            display.println(line1);
            if(previousLogFileExists)
            {
              display.setCursor(0, 16);
              display.println(line2);
              display.setCursor(0, 24);
              display.println(line3);
            }
            display.setCursor(0, 40);
            display.println(line4);

            selectionBox_X = 0;
            selectionBox_Height = 8;
            switch(displaySelectedOption)
            {
              case 0: if(previousLogFileExists){selectionBox_Y = 16; selectionBox_Width = 120; selectionText = "Resume Last Log File";} break;
              case 1: selectionBox_Y = 40; selectionBox_Width = 108; selectionText = "Start New File"; break;
            }
            break;

          }
          case Monitoring:
          {
            float oxygenConcentration = lastReading.oxygenConcentration;//19.54324; //
            float co2Concentration = lastReading.co2Concentration;//821;//ppm
            float ambientTemp =  lastReading.ambientTemp;//17.41232; //degC
            float relativeHumidity = lastReading.relativeHumidity;//85.33235; //%
            float airPressure = lastReading.airPressure;//1014 ;
            float massAirFlow = lastReading.massAirFlow;//143;

            String line1 = "running...";
            String line2 = " O2:";
            line2+= String(oxygenConcentration,1);
            line2+= "% Temp:";
            line2+= String(ambientTemp,1);
            line2+= "C";
            String line3 = "CO2:";
            line3+= String(co2Concentration/10000.0f,2);
            line3+= "% Air:";
            line3+= String(airPressure,0);
            line3+= "hPa";         
            String line4 = " RH:";
            line4+= String(relativeHumidity,1);
            line4+= "% Flow:";
            line4+= String(massAirFlow,0);
            line4+= "SLM";   

            String line5 = "    STOP   MEASURE";

            display.setCursor(0, 0);
            display.println(line1);
            display.setCursor(0, 16);
            display.println(line2);
            display.setCursor(0, 24);
            display.println(line3);
            display.setCursor(0, 32);
            display.println(line4);
            display.setCursor(0, 48);
            display.println(line5);

            selectionBox_Y = 48;
            selectionBox_Height = 8;
            switch(displaySelectedOption)
            {
              case 0: selectionBox_X = 24; selectionBox_Width = 24; selectionText = "STOP"; break;
              case 1: selectionBox_X = 66; selectionBox_Width = 42; selectionText = "MEASURE"; break;
            }
            break;
          }
          case Calibrating:

          break;
        }
      break;


      case SetTime:
      {
        display.setCursor(36, 0);
        display.println("Set Time");
        display.setCursor(6, 24);
        String dateTimeString = "";//time_year1;
        dateTimeString += time_day1;
        dateTimeString += time_day2;
        dateTimeString += "/";
        dateTimeString += time_month1;
        dateTimeString += time_month2;
        dateTimeString += "/";
        dateTimeString += time_year1;
        dateTimeString += time_year2;
        dateTimeString += time_year3;
        dateTimeString += time_year4;

        dateTimeString += " ";
        dateTimeString += time_hr1;
        dateTimeString += time_hr2;
        dateTimeString += ":";
        dateTimeString += time_min1;
        dateTimeString += time_min2;
        dateTimeString += ":";
        dateTimeString += time_sec1;
        dateTimeString += time_sec2;

        display.println(dateTimeString);
        display.setCursor(6, 32);
        display.println("dd MM yyyy hh mm ss");

        selectionBox_Y = 24;
        selectionBox_Width = 6;
        selectionBox_Height = 8;

        switch(displaySelectedOption)
        {
          case 0: selectionBox_X = 6; selectionText = time_day1; break;
          case 1: selectionBox_X = 12; selectionText = time_day2; break;
          case 2: selectionBox_X = 24; selectionText = time_month1; break;
          case 3: selectionBox_X = 30; selectionText = time_month2; break;
          case 4: selectionBox_X = 42; selectionText = time_year1; break;
          case 5: selectionBox_X = 48; selectionText = time_year2; break;
          case 6: selectionBox_X = 54; selectionText = time_year3; break;
          case 7: selectionBox_X = 60; selectionText = time_year4; break;
          case 8: selectionBox_X = 72; selectionText = time_hr1; break;
          case 9: selectionBox_X = 78; selectionText = time_hr2; break;
          case 10: selectionBox_X = 90; selectionText = time_min1; break;
          case 11: selectionBox_X = 96; selectionText = time_min2; break;
          case 12: selectionBox_X = 108; selectionText = time_sec1; break;
          case 13: selectionBox_X = 114; selectionText = time_sec2; break;
        }


        
        // */
        //display.println(displaySelectedOption);
        break;
      }
      case ManualCalibration:
        display.setCursor(10, 0);
        display.println(F("Calibration"));
        break;

      case Error:
        break;
    }

  display.fillRect(selectionBox_X,selectionBox_Y-1,selectionBox_Width,selectionBox_Height+1,SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(selectionBox_X,selectionBox_Y);
  display.println(selectionText);
  display.setTextColor(SSD1306_WHITE);

  display.display();
}


void detectAndProcessInput()
{
    switch(displayState)
    {
      case currentProgramTask:

        switch(progamState)
        {
          case StartUp:

            break;

          case Paused:

            if(buttonMultiPressEventBuffer==2)
            {

              displaySelectedOption = (displaySelectedOption + 1 ) % 2;
              
            }

            if(!previousLogFileExists)
            {
              displaySelectedOption = 1;
            }

            if(buttonMultiPressEventBuffer==1)
            {
              progamState = Monitoring;

              if(displaySelectedOption == 1)
              {
                logFileName = "log";
                logFileName += time_year3;
                logFileName += time_year4;
                logFileName += "-";
                logFileName += time_month1;
                logFileName += time_month2;
                logFileName += "-";
                logFileName += time_day1;
                logFileName += time_day2;
                logFileName += "-";
                logFileName += time_hr1;
                logFileName += time_hr2;
                logFileName += time_min1;
                logFileName += time_min2;
                logFileName += ".csv";
                preferences.putString("lastLogFile", logFileName);
                previousLogFileExists = true;
                //SD.open(logFileName); //Creates new file on SD
              }

            }

            break;

          case Monitoring:

            if(buttonMultiPressEventBuffer==2)
            {
              displaySelectedOption = (displaySelectedOption + 1 ) % 2;
            }
            if(buttonMultiPressEventBuffer==1)
            {
              if(displaySelectedOption == 0)
              {
                progamState = Paused;
              }

            }
            break;

          case Calibrating:

          break;
        }

        if(buttonMultiPressEventBuffer==3)
        {
          displayState = SetTime;
          displaySelectedOption = 0;
        }

        break;

      case SetTime:

        if(buttonMultiPressEventBuffer==3)
        {
          displayState = ManualCalibration;
          displaySelectedOption = 0;
        }
        if(buttonMultiPressEventBuffer==2)
        {
          if(displaySelectedOption==13)
          {
            displaySelectedOption = 0;
          }
          else
          {
            displaySelectedOption++;
          }

        }
        if(buttonMultiPressEventBuffer == 1)
        {
          switch(displaySelectedOption)
          {
          case 0: time_day1 = (time_day1+1) % 4; break;
          case 1: time_day2 = (time_day2+1) % 10; break;
          case 2:  time_month1 = (time_month1+1) % 2; break;
          case 3:  time_month2 = (time_month2+1) % 10; break;
          case 4:  time_year1 = time_year1; break;
          case 5:  time_year2 = time_year2; break;
          case 6:  time_year3 = (time_year3+1) % 10; break;
          case 7:  time_year4 = (time_year4+1) % 10; break;
          case 8:  time_hr1 = (time_hr1+1) % 3; break;
          case 9:  time_hr2 = (time_hr2+1) % 10; break;
          case 10: time_min1 = (time_min1+1) % 6; break;
          case 11: time_min2 = (time_min2+1) % 10; break;
          case 12: time_sec1 = (time_sec1+1) % 6; break;
          case 13: time_sec2 = (time_sec2+1) % 10; break;
          }
        }
        break;


      case ManualCalibration:

        if(buttonMultiPressEventBuffer==3)
        {
          displayState = currentProgramTask;
          displaySelectedOption = 0;
        }
        break;

      case Error:

        break;

    }

    buttonMultiPressEventBuffer = 0;
}

void taskLoop_Monitoring()
{
  unsigned long currentTime = millis();

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

    lastReading = sensorData;

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
}

void taskLoop_Startup()
{
  switch(startupStep)
  {
    case 0: //------------------- STEP 0: Initialise CO2 sensor and check communications --------------------
    

      #if ENABLE_CO2
        if(!SCD4X.begin())
        {
         Serial.println("Communication with CO2 sensor failed, please check connection");
         taskDelay = 1000;
        }
        else
        {
          Serial.println("I2c CO2 connect success!");
          startupStep = 1;
          taskDelay = 1000;
        }
        break;
      #else
        startupStep = 1;
      #endif

    case 1: //-----------------------STEP 1: Configure CO2 sensor -------------------------
    {

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

        startupStep = 2;
        taskDelay = 1000;

        break;
      #else
        startupStep = 2;
      #endif
    }
    case 2: //-------------------------STEP 2: Initialise O2 sensor and check communications ------------------
    

      #if ENABLE_OXYGEN
        if(!oxygen.begin(Oxygen_IICAddress))
        {
         Serial.println("Communication with O2 sensor failed, please check connection");
         taskDelay = 1000;
        }
        else
        {
          Serial.println("I2c O2 connect success!");
          startupStep = 3;
          taskDelay = 1000;
        }
        break;
      #else
        startupStep = 3;
      #endif


    case 3: //---------------------STEP 3: setup RTC ----------------
    

      #if ENABLE_RTC
        if(!rtc.begin())
        {
          Serial.println("Communication with RTC failed, please check connection");
          taskDelay = 1000;
        }
        else
        {
          Serial.println("RTC connect success!");
          rtc.set24Hour(); //Set the rtc to 24h time
          startupStep = 4;
          taskDelay = 1000;
        }
        break;
      #else
        startupStep = 4;
      #endif



    case 4: //---------------------STEP 4: setup SD Card ----------------
     
      #if ENABLE_SD
        if(!SD.begin(chipSelect))
        {
          Serial.println("SD Card failed, or not present");
          taskDelay = 1000;
        }
        else
        {
          Serial.println("SD card initialized.");
          startupStep = 5;
          taskDelay = 1000;
        }
        break;

        logFileName = preferences.getString("lastLogFile", "oh-no!!");
        previousLogFileExists = SD.exists(logFileName);
      #else
        startupStep = 5;
      #endif

    case 5: //---------------------Setup Complete ----------------
      startupComplete = true;
      Serial.println("Startup complete!!");
      taskDelay = 1000;
      progamState = Paused;

  }
}





/*To be added ----------------------------------------------------------------------------------------------*/


  //need to add a callibration for the O2 sensor and other sensors (check against atmospheric)
  //add in OLED functions for readout of values
  //postentially add in external atmostpheric sensor code to calculate the transfer efficiency on the fly
  //add in using built in RGB LED to display any errors with startup or with opperation 
  //add a new file every day, and a running total file, maybe add a reconstrunction if the running total log gets corupted. daily files would rotect the data if the USB is ripped out when being written to
  //add the code to do the 3 way valve 
  //add a feild in the CSV with out of call or bad call data
  //double check the MAF calibration

    //time stamp the datalog - DONE
  
  
  //generally make sure there is error catching, 