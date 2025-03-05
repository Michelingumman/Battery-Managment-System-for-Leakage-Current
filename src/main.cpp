#include <Arduino.h>
// #include <Wire.h>

#include "secrets.h"
#include <gpio_viewer.h>

#include <WiFi.h>
#include <WiFiClient.h>

#include <BlynkSimpleEsp32.h>

// #include <Adafruit_GFX.h>
// #include <Adafruit_SSD1306.h>
#include "image_data.h"
#include <SPI.h>
#include <SdFat.h>
#include <RTClib.h>
#include <string.h>
#include <Adafruit_ADS1X15.h>
// #include <DS3231.h>

// change this value depending on the shunt used
#define SHUNT_SIZE 100.000 //amps / 75mV


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define LEFT 1
#define RIGHT 0

#define INPUT_PIN 32
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On Wemos Lolin32 GPIO21(SDA), GPI022(SCL)

//setup and class for ADC
Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */
// Adafruit_ADS1015 ads;     /* Use this for the 12-bit version */

GPIOViewer gpioViewer;


// RTC DS3231 module
//class declraration
RTC_DS3231 rtc;

// DS3231 rtc(SDA, SCL);

// // SD-card reader
SdFat sd;
SdFile file;
// Change this to the actual pin used for chip select
const int chipSelect = 15;


#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
//Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


/// @brief ////////FUNCTION DECLARATION /////////////
void write_file(float data, int count, String filnamn);
float get_adc_data_in_A();
float get_adc_data_in_V();
// void interface(float amps, int dir);
// void move_arrow(int dir, float amps);
/// @brief //////////////////////////////////////////

BlynkTimer timer;

void myTimer() 
{
  // This function describes what will happen with each timer tick
  // e.g. writing sensor value to datastream V5
  Blynk.virtualWrite(V2, "Detta är ett värde!");  
}


void setup() {

  Serial.begin(115200);
  Serial.setDebugOutput(true);

  gpioViewer.setSamplingInterval(125);

  WiFi.setHostname("VolvoESP32");  // Set custom device name

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }

  // while (!Serial) {}  // wait for board
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  // // Uncomment to use display
  // if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
  //   Serial.println(F("SSD1306 allocation failed"));
  //   for(;;); // Don't proceed, loop forever
  // } else {
  //   Serial.println("SSD1306 allocation success");
  // }

  // // Clear the buffer
  // display.clearDisplay();

  // set analog pin GPIO4 to input to read
  // pinMode(INPUT, INPUT);


// Ported to SdFat from the native Arduino SD library example by Bill Greiman
// On the Ethernet Shield, CS is pin 4. SdFat handles setting SS
/*
 SD card read/write
  
 This example shows how to read and write data to and from an SD card file 	
 The circuit:
 * SD card attached to SPI bus as follows:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 ** CS - pin 4
 
 created   Nov 2010
 by David A. Mellis
 updated 2 Dec 2010
 by Tom Igoe
 modified by Bill Greiman 11 Apr 2011
 This example code is in the public domain.
 	 
 */

  // setup code for ADC
  // The ADC input range (or gain) can be changed via the following
  // functions, but be careful never to exceed VDD +0.3V max, or to
  // exceed the upper and lower limits if you adjust the input range!
  // Setting these values incorrectly may destroy your ADC!
  //                                                                ADS1015  ADS1115
  //                                                                -------  -------
  // ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  // ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
  // ads.setGain(GAIN_TWO);        // 2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
  // ads.setGain(GAIN_FOUR);       // 4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
  // ads.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
  ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV

  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS.");
    while (1);
  }
  

  // RTC setup
  // rtc.begin();
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }
  //gets the time when compiled
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  DateTime time = rtc.now();
  Serial.print("Time right now --> ");
  Serial.print(time.hour());
  Serial.print(":");
  Serial.print(time.minute());
  Serial.print(":");
  Serial.println(time.second());


  // SD card setup

  // Initialize SdFat or print a detailed error message and halt
  // Use half speed like the native library.
  // change to SPI_FULL_SPEED for more performance.
  if (!sd.begin(chipSelect, SPI_HALF_SPEED)) sd.initErrorHalt();

  Serial.println("Jag är här");
  // Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS); // initialize blynk instance elr nått vne riktigt heeh
  // timer.setInterval(500L, myTimer); //data will bne sebt

  Serial.println("Jag är här2");
  // gpioViewer.begin();
}


//  // Code for SD card
//  // Initialize SD card
//   if (!SD.begin(5)) {
//     Serial.println("SD card initialization failed!");
//     return;
//   }
//   Serial.println("SD card initialized successfully!");

// >Variables
float current = 0.0;
int direction = RIGHT;
int direction_before = LEFT;
int direct_log = 0;
int count = 0;
int i = 1;

void loop() {
  DateTime now = rtc.now(); // Hämta aktuell tid
  while(count < 60){
    // Serial.println("Running Blynk");
    // Blynk.run();
    // timer.run(); // run blynktimer set to 500ms
    // Blynk.virtualWrite(V1, "Hello from ESP32");
    
    float data_in_amps = get_adc_data_in_A();
    float data_in_volts = get_adc_data_in_V();
    Serial.print("Volts: ");
    Serial.print(data_in_volts);
    Serial.print("V | Amps: ");
    Serial.print(data_in_amps);
    Serial.println(" A");
    // write to file

    write_file(data_in_amps, count, "Amps "); // fil namn kommer heta "Amps 2025-03-04.txt"
    write_file(data_in_volts, count, "Volts "); // fil namn kommer heta "Volts 2025-03-04.txt"
    

    delay(980); //compensating for runtime 
    count++;
  }

  Serial.println("One minute passed");
  Serial.printf("Datum: %02d-%02d-%04d  Tid: %02d:%02d:%02d\n", 
    now.day(), now.month(), now.year(), 
    now.hour(), now.minute(), now.second());
  
  count = 0; // REseting counter to one more minute
  
  
  
  
  
  
  
  // code for display")
  // // Uncomment to use with display
    // current = analogRead(INPUT_PIN);
    // direct_log = analogRead(INPUT_PIN);
    // Serial.println(direct_log);

    // current = ((current * 200)/4096) - 12;
    // // this means that the voltage is negative
    // if(current < 0) direction = RIGHT;   // which means that the direction of current is charging the battery
    
    // else if(current >= 0) direction = LEFT; // which means that the direction of current is discharging the battery

    
    // //Serial.println(direction)
    // //Serial.println(current);


    // interface(current, direction);

    // // Serial.println(current)
}


float get_adc_data_in_A(){
  int16_t results;
  double Ampmultiplier = 0.0078125; /* ADS1115  @ +/- 6.144V gain (16-bit results) */
  results = ads.readADC_Differential_0_1();
  float amps = float((results * Ampmultiplier * (SHUNT_SIZE / 75.000)));
  return amps;
}


float get_adc_data_in_V(){
  int16_t results;
  double Voltmultiplier = 0.0002696; /* ADS1115  @ +/- 6.144V gain (16-bit results) */
  results = ads.readADC_SingleEnded(2);
  float volts = float(2 * Voltmultiplier * results); //in volts since one step was 0.2696mV / step
  return volts;
}




void write_file(float data, int count, String filnamn) {
  DateTime time = rtc.now();
   
  String date = time.timestamp(DateTime::TIMESTAMP_DATE);
  String txt = ".txt";
  String filename_merged = filnamn + date + txt;
  // Convert filnamn to a char*
  char filename[filename_merged.length() + 1]; // +1 for null terminator
  filename_merged.toCharArray(filename, sizeof(filename));
  
  // Serial.println(date);
  // open the file for write at end like the Native SD library
  if (!file.open(filename, O_RDWR | O_CREAT | O_AT_END)) sd.errorHalt("opening file for write failed");
    

  if(count==0){
    file.println();
    file.print(time.timestamp(DateTime::TIMESTAMP_TIME));
    file.print(" --> ");
  }

  if(count < 59){
    file.print(data);
    file.print(", "); // Add a collon between elements
  }

  if(count == 59) file.println();
    // close the file:
  file.close();

}





// void interface(float amps, int dir){
//   //dir is either LEFT or RI
//   display.clearDisplay();
//   display.drawLine(41, 46, 45, 46, WHITE);
//   display.drawLine(65, 46, 69, 46, WHITE);
//   display.drawLine(71, 46, 75, 46, WHITE);
//   display.drawLine(59, 46, 63, 46, WHITE);
//   display.drawLine(53, 46, 57, 46, WHITE);
//   display.drawLine(47, 46, 51, 46, WHITE);
//   display.drawBitmap(79, 16, image_volvo_bits, 50, 50, WHITE);
//   display.drawBitmap(78, 30, image_Layer_18_bits, 51, 17, WHITE);
//   display.drawBitmap(0, 31, image_battery__2__bits, 40, 30, WHITE);
//   move_arrow(dir, amps);
//   display.display();
// }


// function that moves the arrow either right or left showing the current direction
// this function below should be able to be run in the background of the code
// void move_arrow(int dir, float amps){
//   int x = 38;
//   int x_slut = 69;

//   if(dir == RIGHT){
//   x = 38;
//     display.setTextColor(WHITE);
//     display.setTextSize(1);
//     display.setCursor(3, 6);
//     display.setTextWrap(false);
//     display.print("Discharging:");
//     display.print(amps);
//     display.print(" A");
//     for(int i = 0; i < 31; i++){
//       display.drawBitmap(x, 37, image_Pin_arrow_right_9x7_bits, 9, 7, WHITE);
//       display.display();
//       delay(10);
//       display.drawBitmap(x, 37, image_Pin_arrow_right_9x7_bits, 9, 7, BLACK);
//       x++;
//       if(x==x_slut) x = 38;
//     }
//   }

//   else if(dir == LEFT){
//   x = x_slut;
//     display.setTextColor(WHITE);
//     display.setTextSize(1);
//     display.setCursor(3, 6);
//     display.setTextWrap(false);
//     display.print("Charging:");
//     display.print(amps);
//     display.print(" A");
//     for(int i = 0; i < 31; i++){
//       display.drawBitmap(x, 37, image_Pin_arrow_left_9x7_bits, 9, 7, WHITE);
//       display.display();
//       delay(10);
//       display.drawBitmap(x, 37, image_Pin_arrow_left_9x7_bits, 9, 7, BLACK);
//       x--;
//       if(x==38) x = x_slut;
//     }
//   }
// }


