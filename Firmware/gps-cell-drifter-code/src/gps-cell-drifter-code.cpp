
#include "Particle.h"

void setup();
void loop();

// function prototypes for saving and serial printing (eventually publishing also)
void createDataString();
void printToFile();
void serialPrintGPSTime();
void serialPrintGPSLoc();
void publishData();

long real_time;
int millis_now;


// ------------------ SD SPI Configuration Details --------------------------------
#include "SdFat.h"
const int SD_CHIP_SELECT = D5;
SdFat SD;

char filename[] = "YYMMDD00.csv"; // template filename (year, month, day, 00–99 file number for that day)
bool filenameCreated = false;

// ---------------------------- GPS ----------------------------------------------
#include <Adafruit_GPS.h>
#define GPSSerial Serial1
Adafruit_GPS GPS( & GPSSerial);
uint32_t timer = millis();

//-------------------------- LED Setup -------------------------------------------
const pin_t MY_LED = D7; // blink to let us know you're alive
bool led_state = HIGH; // starting state


// Global objects; TODO: save power stats!
FuelGauge batteryMonitor;

// String for printing and publishing
String dataString = "";
const char * eventName = "whereAmI";

// Define whether to publish
#define PUBLISHING 0
const unsigned long PUBLISH_PERIOD_MS = 300000; // milliseconds between publish events
const unsigned long DATALOG_PERIOD_MS = 1000; // milliseconds between datalog events

// To use Particle devices without cloud connectivity
SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_THREAD(ENABLED);


//===============================================================================
// INITIALIZATION
//===============================================================================
void setup() {

  if (PUBLISHING == 1) {
    Particle.connect();
  } else {
    Cellular.off(); // Turn off cellular for preliminary testing
  }

    pinMode(MY_LED, OUTPUT);


    //Serial.begin(9600);
    Serial.begin(115200);
    Serial.println("GPS drifter test");

    // Set up GPS
    GPS.begin(9600);

    // uncomment next line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
    GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);

    // uncomment next line to turn on only the "minimum recommended" data
    //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);

    // Set the update rate
    GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); // 1 Hz update rate
    // For the parsing code to work nicely and have time to sort thru the data, and print it out we don't suggest using anything higher than 1 Hz
    

    // Initialize the SD library
    if (!SD.begin(SD_CHIP_SELECT, SPI_FULL_SPEED)) {
        Serial.println("failed to open card");
        return;
    }

}


//===============================================================================
// MAIN
//===============================================================================
void loop() {

    // read data from the GPS in the 'main loop'
    char c = GPS.read();

    // if a sentence is received, we can check the checksum, parse it...
    if (GPS.newNMEAreceived()) {
        if (!GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
            return; // we can fail to parse a sentence in which case we should just wait for another
    }

    // approximately every second or so, print out the current stats
    if (millis() - timer > DATALOG_PERIOD_MS) {
        // reset the timer
        // CRITICAL WARNING: watch this for rollover back to zero once millis maxes out
        timer = millis(); 

        serialPrintGPSTime();

        // If GPS gets a fix, print out and save good data
        if (GPS.fix) {  
            // Blink to let us know you're alive
            led_state = !led_state;
            digitalWrite(MY_LED, led_state); // turn the LED on (HIGH is the voltage level)

            serialPrintGPSLoc();
        }

        createDataString();
        printToFile();

        

        if (PUBLISHING == 1) {
          if (millis() - timer > PUBLISH_PERIOD_MS){
            publishData();
          } // no else needed; just keep rolling
        }
    }

}

void createDataString(){

  dataString = ""; // Initialize empty string each time to prevent reprinting old data if no new data arrive

  // Date
  dataString += String(GPS.month, DEC) + "/";
  dataString += String(GPS.day, DEC) + "/20";
  dataString += String(GPS.year, DEC) + ",";

  // Time
  dataString += String(GPS.hour, DEC) + ":";
  if (GPS.minute < 10) dataString += "0";
  dataString += String(GPS.minute, DEC) + ":";
  if (GPS.seconds < 10) dataString += "0";
  dataString += String(GPS.seconds, DEC) + ".";
  if (GPS.milliseconds < 10) {
      dataString += "00";
  } else if (GPS.milliseconds > 9 && GPS.milliseconds < 100) {
      dataString += "0";
  }
  dataString += String(GPS.milliseconds) + ",";

  // Elapsed Time
  dataString += String(millis() / 1000) + ",";

  // Location
  dataString += String(GPS.latitude, 4) + ",";
  dataString += String(GPS.lat) + ","; // N or S
  dataString += String(GPS.longitude, 4) + ",";
  dataString += String(GPS.lon) + ","; // E or W

  // Altitude
  dataString += String(GPS.altitude) + ",";

  // Speed
  dataString += String(GPS.speed) + ",";

  // Angle
  dataString += String(GPS.angle);
}

/* ---------------------- PRINT TO SD CARD FUNCTION ---------------------- */
void printToFile() {

  if (!filenameCreated) {
    // Get year, month, and day for filename
    int filenum = 0; // start at zero and increment by one if file exists
    sprintf(filename, "%02d%02d%02d%02d.csv", GPS.year, GPS.month, GPS.day, filenum);

    // Check for existence of filename with current filenum
    while (SD.exists(filename)) {
      filenum++;
      sprintf(filename, "%02d%02d%02d%02d.csv", GPS.year, GPS.month, GPS.day, filenum);
    }
    filenameCreated = true;
  }

  Serial.println(filename);

  // Create filename
  // Open the file: SPI SD comms
  File dataFile = SD.open(filename, FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {

    // Print the entire formatted string
    dataFile.println(dataString);

    dataFile.close();
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  }
}

void serialPrintGPSTime() {
  Serial.print("\nTime: ");
  
  // Hour
  if (GPS.hour < 10) {
    Serial.print('0');
  }
  Serial.print(GPS.hour, DEC);

  Serial.print(':');

  // Minute
  if (GPS.minute < 10) {
    Serial.print('0');
  }
  Serial.print(GPS.minute, DEC);

  Serial.print(':');

  // Seconds
  if (GPS.seconds < 10) {
    Serial.print('0');
  }
  Serial.print(GPS.seconds, DEC);
  Serial.print('.');
  if (GPS.milliseconds < 10) {
    Serial.print("00");
  } else if (GPS.milliseconds > 9 && GPS.milliseconds < 100) {
    Serial.print("0");
  }
  Serial.println(GPS.milliseconds);
  
  Serial.print("Date: ");
  Serial.print(GPS.month, DEC);
  Serial.print('/');
  Serial.print(GPS.day, DEC);
  Serial.print("/20");
  Serial.println(GPS.year, DEC);
  
  Serial.print("Fix: ");
  Serial.print((int) GPS.fix);
  
  Serial.print(" quality: ");
  Serial.println((int) GPS.fixquality);
}

void serialPrintGPSLoc() {
  Serial.print("Location: ");
  Serial.print(GPS.latitude, 4);
  Serial.print(GPS.lat);
  Serial.print(", ");
  Serial.print(GPS.longitude, 4);
  Serial.println(GPS.lon);
}

void publishData() {

      //connect particle to the cloud
      if (Particle.connected() == false) {
        Particle.connect();
        Log.info("Trying to connect");
      }

      // If connected, publish data buffer
      if (Particle.connected()) {

        Log.info("publishing data");

        // bool (or Future) below requires acknowledgment to proceed
        bool success = Particle.publish(eventName, dataString, 60, PRIVATE, WITH_ACK);
        Log.info("publish result %d", success); 

        
      }
      // If not connected after certain amount of time, go to sleep to save battery
      else {
          Log.info("not connecting; proceed without publishing");
      }
}