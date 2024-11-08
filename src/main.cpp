#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "image_data.h"
#include <SPI.h>
#include <SdFat.h>
#include <RTClib.h>
#include <string.h>
#include <Adafruit_ADS1X15.h>
//#include <DS3231.h>

// change this value depending on the shunt used
#define SHUNT_SIZE 50 //amps / 75mV


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
//Adafruit_ADS1015 ads;     /* Use this for the 12-bit version */


// RTC DS3231 module
//class declraration
RTC_DS3231 rtc;
//DS3231  rtc(SDA, SCL);

// SD-card reader
SdFat sd;
SdFile file;
// Change this to the actual pin used for chip select
const int chipSelect = 5;


#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


/// @brief ////////FUNCTION DECLARATION /////////////
void write_file(float data, int count);
float get_adc_data();
void interface(float amps, int dir);
void move_arrow(int dir, float amps);
/// @brief //////////////////////////////////////////

void setup() {

  Serial.begin(115200);
  while (!Serial) {}  // wait for board
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
  rtc.begin();

  if (!sd.begin(chipSelect, SPI_HALF_SPEED)) sd.initErrorHalt();
}



// >Variables
float current = 0.0;
int direction = RIGHT;
int direction_before = LEFT;
int direct_log = 0;
int count = 0;

void loop() {
  
  while(count < 60){
    float data = get_adc_data();
    Serial.println(data);
    // write to file
    write_file(data, count);

    delay(999.96); //compensating for runtime 
    count++;
  }

  count = 0;
  
}


float get_adc_data(){
  int16_t data;
  float multiplier = 0.0078125F; /* ADS1115  @ +/- 6.144V gain (16-bit results) */
  int results = ads.readADC_Differential_0_1();
  return (results * multiplier * SHUNT_SIZE / 75 * 1000);
}


void write_file(float data, int count) {
  DateTime time = rtc.now();

  String date = time.timestamp(DateTime::TIMESTAMP_DATE);
  String txt = ".txt";
  String filename_merged = date + txt;
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





void interface(float amps, int dir){
  //dir is either LEFT or RI
  display.clearDisplay();
  display.drawLine(41, 46, 45, 46, WHITE);
  display.drawLine(65, 46, 69, 46, WHITE);
  display.drawLine(71, 46, 75, 46, WHITE);
  display.drawLine(59, 46, 63, 46, WHITE);
  display.drawLine(53, 46, 57, 46, WHITE);
  display.drawLine(47, 46, 51, 46, WHITE);
  display.drawBitmap(79, 16, image_volvo_bits, 50, 50, WHITE);
  display.drawBitmap(78, 30, image_Layer_18_bits, 51, 17, WHITE);
  display.drawBitmap(0, 31, image_battery__2__bits, 40, 30, WHITE);
  move_arrow(dir, amps);
  display.display();
}


// function that moves the arrow either right or left showing the current direction
// this function below should be able to be run in the background of the code
void move_arrow(int dir, float amps){
  int x = 38;
  int x_slut = 69;

  if(dir == RIGHT){
  x = 38;
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(3, 6);
    display.setTextWrap(false);
    display.print("Discharging:");
    display.print(amps);
    display.print(" A");
    for(int i = 0; i < 31; i++){
      display.drawBitmap(x, 37, image_Pin_arrow_right_9x7_bits, 9, 7, WHITE);
      display.display();
      delay(10);
      display.drawBitmap(x, 37, image_Pin_arrow_right_9x7_bits, 9, 7, BLACK);
      x++;
      if(x==x_slut) x = 38;
    }
  }

  else if(dir == LEFT){
  x = x_slut;
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(3, 6);
    display.setTextWrap(false);
    display.print("Charging:");
    display.print(amps);
    display.print(" A");
    for(int i = 0; i < 31; i++){
      display.drawBitmap(x, 37, image_Pin_arrow_left_9x7_bits, 9, 7, WHITE);
      display.display();
      delay(10);
      display.drawBitmap(x, 37, image_Pin_arrow_left_9x7_bits, 9, 7, BLACK);
      x--;
      if(x==38) x = x_slut;
    }
  }
}


