/*
 ** BikeSense Distance Sensor and Data Logger 
 ** Version 0.2 by Ian Walker, University of Bath
 ** Available under the Creative Commons BY-NC-SA terms
 ** http://creativecommons.org/licenses/by-nc-sa/2.0/uk/
 
 Changelog
 Version 0.1 
 Trial version
 
 Version 0.2 
 Writes to SD rather than serial output
 Allows a long press to cancel recording and reset
 
 This code is intended to be modular so that, if others use it,
 and employ some different hardware to me, the number of changes will be 
 minimal. Most of the hardware interactions are in self-contained
 functions at the bottom and it is only the functions related to your
 hardware that you will need to change
 
 SIGNALS and MODES
 *Startup sequence is: 
 - 5 fast flashes
 - long flash = realtime clock is working
 - long flash = SD card found
 - 5 fast flashes
 *Then goes into "ready" mode - two flashes and a pause
 *Any problems, it will lock up and signal SOS
 *Press button for around 0.5s to enter recording mode:
 - 6 steady flashes = new file successfully created
 - steady = recording
 *Hold for 10s to cancel recording; goes back to "ready" mode
 
 */

// Libraries and definitions
#include <Wire.h>
#include "Ultrasonic.h" // from http://arduino-info.wikispaces.com/UltraSonicDistance
#include "RTClib.h" // from http://www.ladyada.net/make/logshield/download.html
#include <SD.h> 

#define buttonPin 2 // reads the handlebar button
#define lightPin 4  // controls the signal light
#define sampleRate 10 // in Hertz - might want to alter this
#define syncInterval 5000 // ms between writes to disk
#define buttonTimeToStopRecording 10000 // how long to hold down the button to cancel recording in ms
#define CORRECTION 2 // adjustment to calibrate the sensor. 2 would mean add 2 cm
#define maxDistanceOfInterest 400 // ignore any readings >= than this
#define minDistanceOfInterest 19 // ignore any reading <= this. NB Likely defined by sensor limitations

RTC_DS1307 RTC; // initalize real time clock


// Set up global variables that won't need changing by the user
// These just hold and pass information for the program
DateTime nowTime; // for real time clock readings
unsigned long lastReadingTime = 0; // for internal reading timer (working with millis)
unsigned long readingTime; // will indicate the millis of the current reading
unsigned long lastSyncTime = 0; // when was the file last written?
unsigned long lastPressTime = 0; // when was button last pressed? For reset-to-standby ability
byte buttonDown;
byte problem = 0; // to indicate trouble to the user
byte recording = 0; // to indicate whether data collection has begun
const int chipSelect = 10;   // 10 for the Adafruit SD card system
File logfile; // file pointer
int lastReading; // FIXME - is this actually being used?


/* INITIAL CALCULATIONS */
// Calculate the delay between sensor readings
// I calculate this here as it's easier for users to think in HZ
// and set that above
unsigned long samplePause = 1000 / sampleRate;


/* *************************
 ** Initial, one-off setup **
 ************************* */
void setup() {

  pinMode(buttonPin, INPUT); // button will be used as an input
  pinMode(lightPin, OUTPUT); // the button's lamp will be used as an output
  // NB The button I used had a lamp built in. But this could 
  // easily be a separate LED (and resistor) if your button doesn't
  // have one built in

  Wire.begin();
  RTC.begin();

  flashLight(5, 50); // we've got this far okay, first flashes are distinctive to mark beginning
  delay(300);

  // Check RTC is okay
  if (! RTC.isrunning()) {   
    problem = 1;
    return; // don't do anything more
  } 
  else {
    flashLight(1, 500); // Clock system is okay
  }

  // initalize SD card
  pinMode(10, OUTPUT);

  // see if the card is present and can be initialized:
  if (! SD.begin(chipSelect)) {
    problem = 1;
    return;     // don't do anything more
  } 
  else {
    flashLight(1, 500); // Sd card is present and correct
  }

  // setup complete
  flashLight(5, 50); // we're all ready to go!
  delay(500); // short delay so the READY signal is distinct from the waiting-to-begin signal 
}


/* ************
 ** MAIN LOOP **
 ************ */
void loop()
{
  unsigned long nowDistance;
  byte buttonReading;

  // error reporting system
  if (problem) { // If something is wrong, do nothing but alert the user
    sos();
  }

  // If we're not currently recording, slow flash to tell user
  if (! problem && ! recording) {
    readySignal();
  }

  // Each loop, grab the timings
  nowTime = timestamp();  // Get the current real time
  readingTime = millis(); // and the millis time for checking inter-reading delay

  // Check the button
  buttonReading = digitalRead(buttonPin); 

  // if we're not recording and the button's pressed, we start recording
  if (! problem && ! recording && buttonReading == HIGH) { 
    recording = 1; 
    newfile();
    // Did the file open successfully?
    if(! logfile) {
      problem = 1;
    } 
    else {
      flashLight(6,200); // indicate the new file was created successfully
      // Output the datafile headings
      logfile.println("milliseconds,date_time,distance,button");
    }
  }

  // if we're recording and the button is pressed turn the light off to show this
  if (! problem && recording && buttonReading == HIGH) {
    digitalWrite(lightPin, LOW); // light off when button is down
    if (! buttonDown) { // if we've JUST pressed the button
      lastPressTime = readingTime; // note the time
      buttonDown = 1; // note that the button has been pressed
    }
  } 
  else if (recording) {
    digitalWrite(lightPin, HIGH); // light on when button is up
    buttonDown = 0;
  }

  // big reset if button is down for a long time
  if (buttonDown && (readingTime - lastPressTime) >= buttonTimeToStopRecording) {
    recording = 0; // we're no longer recording
    logfile.flush(); // write everything outstanding to the file
    
    // go into a loop that fakes the "ready" signal until the button is released
    // as we don't want to go any further until user lets go
    while (buttonDown) {
      buttonReading = digitalRead(buttonPin);
      if (buttonReading == HIGH) {
        buttonDown = 1;
      } else {
        buttonDown = 0;
      }
      readySignal();
    }
  }

  // if we're recording and ready for the next reading
  if (! problem && recording && (readingTime - lastReadingTime) >= samplePause) { 
    nowDistance = distanceReading(); // Get the range in centimetres

    nowDistance = nowDistance + CORRECTION; // re-calibrate it

    logfile.print(readingTime);
    logfile.print(",");
    logfile.print(nowTime.year());
    logfile.print("-");
    leadingZero(nowTime.month());
    logfile.print("-");
    leadingZero(nowTime.day());
    logfile.print("T");
    leadingZero(nowTime.hour());
    logfile.print("-");
    leadingZero(nowTime.minute());
    logfile.print("-");
    leadingZero(nowTime.second());
    logfile.print(",");         // CSV comma
    if(nowDistance > minDistanceOfInterest && nowDistance < maxDistanceOfInterest) { // if measured distance is below extreme cut-offs
      logfile.print(nowDistance);   // write the distance datum
    } 
    else if (nowDistance >= maxDistanceOfInterest) { // But if the reading is beyond the sensor's limit
      logfile.print("999");   // write a marker to show an Out of Range reading
    }
    else if (nowDistance <= minDistanceOfInterest) { // and finally if reading is too low
      logfile.print("000");  // bottom out of range
    }
    logfile.print(",");
    logfile.println(buttonReading); // is the button down?

    lastReadingTime = readingTime;     // note when we last wrote to the file
    lastReading = nowDistance;
  } // end of reading IF

  // check if we need to write to disk
  if (! problem && recording && (readingTime - lastSyncTime) >= syncInterval) { 
    logfile.flush();
    lastSyncTime = readingTime;
  } // end of sync IF
}


/* ****************
 ** The functions **
 **************** */

// Function to provide a timestamp
DateTime timestamp() {
  // Written to work with the real-time clock that is integrated into an
  // Adafruit data logger shield. 
  // Should also work with stand-alone clocks such as the Sparkfun RTC module
  // http://proto-pic.co.uk/real-time-clock-module/
  // as it's the same chip, provided the same arduino pins are used

  DateTime nowTime = RTC.now();
  return(nowTime);
}

// function to query the distance sensor
// and return a result in cm
// A little help from http://www.arduino.cc/playground/Main/MaxSonar
unsigned long distanceReading() {
  unsigned long reading;
  unsigned long cm;

  int pulsePin = 7;
  reading = pulseIn(pulsePin, HIGH);
  
  cm = reading/58; // 58 microsecs per cm

  // additional, empirical correction
  cm = cm / 1.24;
  return(cm);
}

// Prints number to file with a leading zero if necessary. FIXME - make this a proper
// function that returns the text rather than print it directly
void leadingZero(uint8_t x) {
  if(x < 10) {
    logfile.print(0);
  }
  logfile.print(x);
}

// Flashes the signal light "x" times for "time" ms
void flashLight(int x, int time) {
  for (int i = 0; i < x; i++) {
    digitalWrite(lightPin, HIGH);
    delay(time);
    digitalWrite(lightPin, LOW);
    delay(time);
  }
}

// SEEMS NOT TO BE USED!!
void offLight(int x, int time) {
  for (int i = 0; i < x; i++) {
    digitalWrite(lightPin, LOW);
    delay(time);
    digitalWrite(lightPin, HIGH);
    delay(time);
  }
}

// error signal
void sos() {
  flashLight(3,150);
  delay(150);
  flashLight(3,400);
  flashLight(3,150);
  delay(500);
}

// Create new file on SD card
File newfile() {
  nowTime = timestamp(); // get the time, which we'll use for the filename

  String filename = String();
  if (nowTime.month() < 10) { // for goodness sake do a proper leading zero function that returns value!
    filename+= "0";
  }
  filename += nowTime.month();
  if (nowTime.day() < 10) { 
    filename+= "0";
  }
  filename += nowTime.day();
  if (nowTime.hour() < 10) { 
    filename+= "0";
  }
  filename += nowTime.hour();
    if (nowTime.minute() < 10) { 
    filename+= "0";
  }
  filename += nowTime.minute();
  filename += ".csv";

  // convert file string to character array and open the file for writing
  char filename2[30];
  filename.toCharArray(filename2, 30);
  logfile = SD.open(filename2, FILE_WRITE);
  //  return(logfile);
}

// signal ready to use
void readySignal() {
  flashLight(2, 100);
  delay(700);
}


