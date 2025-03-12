/** CREATED BY ADAM AND MATTIAS MICHELIN
 * 
 * Battery Management System for Leakage Current Monitoring
 * 
 * This ESP32-based system monitors battery voltage and current,
 * storing timestamped readings to SD card files for later analysis.
 * When WiFi connection is available, data can be accessed remotely.
 * 
 * Features:
 * - High-precision ADC measurements for voltage and current
 * - RTC-synchronized timestamps
 * - SD card data logging
 * - Web interface for accessing logged data files
 * - MQTT data upload when connected to WiFi
 * - OTA updates via WiFi
 * 
 * Hardware:
 * - ESP32 development board
 * - ADS1115 16-bit ADC
 * - DS3231 RTC module
 * - SD card module
 * - Current shunt (100A/75mV)
 */

// ===== FEATURE FLAGS =====
// Set to 1 to enable display support, 0 to disable
#define ENABLE_DISPLAY 0
// Set to 1 to enable MQTT data upload, 0 to disable
#define ENABLE_MQTT 1
// Set to 1 to enable SD card testing mode (writes test data every second)
#define SD_CARD_TEST_MODE 0

#define UPDATE_RTC_TIME 0

#define VOLTAGE_OFFSET 0.4
#define CURRENT_OFFSET 0.0

// Define LED_BUILTIN for ESP32 (usually GPIO2)
#define LED_BUILTIN 2

// ===== INCLUDE LIBRARIES =====
#include <Arduino.h>
#include <Wire.h>
#include "secrets.h"        // Contains WiFi credentials (WIFI_SSID, WIFI_PASS) and MQTT details

// Network libraries
#include <WiFi.h>
#include <WiFiClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>     // OTA update functionality

#if ENABLE_MQTT
#include <PubSubClient.h>   // MQTT client
#endif

// Sensor and storage libraries
#include <SPI.h>
#include <SdFat.h>          // SD card file system
#include <RTClib.h>         // Real-time clock
#include <string.h>
#include <Adafruit_ADS1X15.h> // High-precision ADC

// Optional display libraries
#if ENABLE_DISPLAY
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif
#include "image_data.h"     // For display images (if enabled)

// ===== CONFIGURATION =====
// Display settings
#if ENABLE_DISPLAY
#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET     -1   // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#endif

// Calibration values
#define SHUNT_SIZE 100.000  // Current shunt rating: 100A / 75mV

// Direction constants
#define LEFT 1
#define RIGHT 0

// ADC settings
#define INPUT_PIN 32        // Analog input pin (if using ESP32 ADC)

// SD card settings
const int chipSelect = 15;  // SD card chip select pin

// MQTT settings
#if ENABLE_MQTT
const char* mqtt_server = MQTT_SERVER;   // From secrets.h
const int mqtt_port = MQTT_PORT;         // From secrets.h
const char* mqtt_user = MQTT_USER;       // From secrets.h
const char* mqtt_pass = MQTT_PASS;       // From secrets.h
const char* mqtt_client_id = "ESP32_BatteryMonitor";
const char* mqtt_topic_current = "battery/data/current";
const char* mqtt_topic_voltage = "battery/data/voltage";
const char* mqtt_topic_status = "battery/status";
#endif

// Timing settings
const unsigned long WIFI_RETRY_INTERVAL = 60000;  // Try to reconnect WiFi every minute
const unsigned long WIFI_CONNECT_TIMEOUT = 10000; // 10 seconds timeout for WiFi connection

// ===== OBJECT INITIALIZATION =====
Adafruit_ADS1115 ads;       // 16-bit ADC object
RTC_DS3231 rtc;             // Real-time clock object
SdFat sd;                   // SD card filesystem
SdFile file;                // File object for writing
AsyncWebServer server(80);  // Web server for OTA updates and file access

#if ENABLE_MQTT
WiFiClient espClient;       // WiFi client for MQTT
PubSubClient mqtt(espClient); // MQTT client
#endif

#if ENABLE_DISPLAY
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#endif

// ===== GLOBAL VARIABLES =====
// OTA update variables
unsigned long ota_progress_millis = 0;

// Battery monitoring variables
float current = 0.0;
int direction = RIGHT;
int direction_before = LEFT;
int direct_log = 0;
int count = 0;
int i = 1;

// Connection state variables
bool wifi_connected = false;
unsigned long last_wifi_attempt = 0;

// SD card monitoring
unsigned long last_sd_check = 0;
bool sd_initialized = false;
unsigned long last_data_written = 0;

// ===== FUNCTION DECLARATIONS =====
// OTA callback functions
void onOTAStart();
void onOTAProgress(size_t current, size_t final);
void onOTAEnd(bool success);

// Data collection and storage functions
float get_adc_data_in_A();    // Get current reading in Amperes
float get_adc_data_in_V();    // Get voltage reading in Volts
void write_file(float data, int count, String filename); // Log data to SD card
#if SD_CARD_TEST_MODE
void test_sd_card_write();    // Test function for basic SD card writing
#endif

// WiFi and MQTT functions
void check_wifi_connection();            // Check and attempt to reconnect WiFi

#if ENABLE_MQTT
bool connect_mqtt();                     // Connect to MQTT broker
void publish_data_point(float current, float voltage, DateTime timestamp); // Publish single data point
#endif

// Web server functions
void setup_web_server();                 // Setup web server routes
String list_files(String path);          // List all files in directory
String get_file_size(size_t bytes);   // Get file size as string

// Display functions
#if ENABLE_DISPLAY
void interface(float amps, int dir);  // Update display interface
void move_arrow(int dir, float amps); // Animate the direction arrow
#endif

// SD card functions
bool init_sd_card();
bool check_sd_card();

// ===== OTA CALLBACK IMPLEMENTATIONS =====
/**
 * Called when OTA update begins
 */
void onOTAStart() {
  Serial.println("OTA update started!");
}

/**
 * Called periodically during OTA update process
 */
void onOTAProgress(size_t current, size_t final) {
  // Log progress every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress: %u of %u bytes (%.1f%%)\n", 
                 current, final, (current * 100.0) / final);
  }
}

/**
 * Called when OTA update completes
 */
void onOTAEnd(bool success) {
  if (success) {
    Serial.println("OTA update completed successfully!");
  } else {
    Serial.println("Error during OTA update!");
  }
}

#if SD_CARD_TEST_MODE
/**
 * Simple test function to verify SD card writing is working
 * Just writes zeros to a test file every second
 */
void test_sd_card_write() {
  static unsigned long lastWrite = 0;
  static int counter = 0;
  
  // Write a value every second
  if (millis() - lastWrite >= 1000) {
    lastWrite = millis();
    counter++;
    
    // Create a simple test file
    const char* testFileName = "SDTEST.TXT";
    
    Serial.print("Writing to test file: ");
    Serial.print(testFileName);
    Serial.print(" | Counter: ");
    Serial.println(counter);
    
    // Try to open the file for writing (append mode)
    if (!file.open(testFileName, O_RDWR | O_CREAT | O_AT_END)) {
      Serial.println("ERROR: Failed to open test file for writing!");
      return;
    }
    
    // Write a simple value (just the counter)
    file.print(counter);
    file.println(", 0");
    
    // Close the file to ensure data is saved
    file.close();
    
    // Verify file exists and has data
    SdFile checkFile;
    if (checkFile.open(testFileName, O_READ)) {
      Serial.print("Test file size: ");
      Serial.print(checkFile.fileSize());
      Serial.println(" bytes");
      
      // Read and display the file contents
      Serial.println("File contents:");
      char buffer[100];
      int idx = 0;
      int c;
      
      // Read up to 100 characters or end of file
      while ((c = checkFile.read()) >= 0 && idx < sizeof(buffer) - 1) {
        buffer[idx++] = (char)c;
      }
      buffer[idx] = '\0';  // Null-terminate the string
      
      Serial.println(buffer);
      checkFile.close();
    } else {
      Serial.println("WARNING: Could not verify test file after writing!");
    }
    
    // Flash the built-in LED to indicate a write cycle
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
  }
}
#endif

// ===== SETUP FUNCTION =====
void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("\n=== Battery Management System ===");
  Serial.println("Initializing...");

  // Set up built-in LED for write indication
  pinMode(LED_BUILTIN, OUTPUT);

  // Initialize WiFi connection
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("VolvoESP32");  // Set custom device name
  check_wifi_connection();         // Initial WiFi connection attempt

  // Initialize ADS1115 ADC
  Serial.println("Initializing ADC...");
  // Set ADC gain for highest precision in our voltage range
  // GAIN_SIXTEEN: +/- 0.256V range with 0.0078125mV per bit
  ads.setGain(GAIN_SIXTEEN);

  if (!ads.begin()) {
    Serial.println("ERROR: Failed to initialize ADS1115!");
    while (1) {
      delay(100); // Halt system if ADC initialization fails
    }
  }
  Serial.println("ADC initialized successfully");

  // Initialize RTC module
  Serial.println("Initializing RTC...");
  if (!rtc.begin()) {
    Serial.println("ERROR: Couldn't find RTC!");
    while (1) {
      delay(100); // Halt system if RTC initialization fails
    }
  }

  #if UPDATE_RTC_TIME
    // Set RTC to compilation time

    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__ + 1 minute))) 
    //since the codebase is getting large then we need to compensate for the build time ;)
    rtc.adjust(DateTime(F(__DATE__), F("01:41:20")));
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  #endif

  DateTime time = rtc.now();
  Serial.print("Current time: ");
  Serial.printf("%02d:%02d:%02d\n", time.hour(), time.minute(), time.second());

  // Initialize SD card
  Serial.println("Initializing SD card...");
  if (!sd.begin(chipSelect, SPI_HALF_SPEED)) {
    Serial.println("ERROR: SD card initialization failed!");
    while (1) {
      // Flash LED to indicate SD card error
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
    }
  }
  Serial.println("SD card initialized successfully");

  // Check if the SD card is writable by creating a test file
  SdFile testFile;
  if (!testFile.open("SDTEST.TXT", O_RDWR | O_CREAT | O_TRUNC)) {
    Serial.println("ERROR: Cannot create test file on SD card!");
    while (1) {
      // Flash LED to indicate error
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
    }
  }
  
  // Write initial test data
  testFile.println("SD card test file created at startup");
  testFile.println("Current time: " + time.timestamp(DateTime::TIMESTAMP_FULL));
  testFile.close();
  Serial.println("Test file created successfully");

  // Verify we can read the file back
  if (testFile.open("SDTEST.TXT", O_READ)) {
    size_t fileSize = testFile.fileSize();
    Serial.print("Test file size: ");
    Serial.print(fileSize);
    Serial.println(" bytes");
    testFile.close();
    
    if (fileSize == 0) {
      Serial.println("ERROR: Test file is empty - write failed!");
      while (1) {
        // Flash LED to indicate error
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
      }
    }
  } else {
    Serial.println("ERROR: Could not read back test file!");
    while (1) {
      // Flash LED to indicate error
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
    }
  }

  // Setup web server for file browsing and OTA updates
  setup_web_server();

#if ENABLE_MQTT
  // Setup MQTT client
  mqtt.setServer(mqtt_server, mqtt_port);
  if (wifi_connected) {
    connect_mqtt();
  }
#endif

#if ENABLE_DISPLAY
  // Initialize OLED display
  Serial.println("Initializing display...");
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("ERROR: SSD1306 display initialization failed");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Battery Monitor");
    display.println("Initializing...");
    display.display();
    Serial.println("Display initialized successfully");
  }
#endif

  Serial.println("Setup complete!");
#if SD_CARD_TEST_MODE
  Serial.println("RUNNING IN TEST MODE - Writing to test file every second");
#endif
}

int previous_time = 0;
int time_now = 0;

// ===== MAIN LOOP =====
void loop() {
#if SD_CARD_TEST_MODE
  // Run the simple SD card write test instead of the normal data collection
  test_sd_card_write();
  
  // Handle OTA updates if WiFi connected
  if (wifi_connected) {
    ElegantOTA.loop();
  }
  
  // Try to reconnect WiFi periodically if disconnected
  if (millis() - last_wifi_attempt > WIFI_RETRY_INTERVAL) {
    check_wifi_connection();
  }

  // A short delay to avoid hammering the loop
  delay(10);
#else
  // Handle OTA updates if WiFi connected
  if (wifi_connected) {
    ElegantOTA.loop();
  }

  // Get current date/time
  DateTime now = rtc.now();
  
  // Try to reconnect WiFi periodically if disconnected
  if (millis() - last_wifi_attempt > WIFI_RETRY_INTERVAL) {
    check_wifi_connection();
  }
  
  // Collect and log data every minute (60 samples at ~1 second intervals)
  while (count < 60) {
    //if one seconds passed then run the data logging, else wait until one second
    time_now = millis();
    if((time_now - previous_time) >= 1000){
      previous_time = millis();

      // Read voltage and current values
      float data_in_amps = get_adc_data_in_A();
      float data_in_volts = get_adc_data_in_V();
              
      // Get current time
      DateTime timestamp = rtc.now();
              
      // Display readings on serial monitor
      Serial.printf("Volts: %.2fV | Amps: %.2fA | WiFi: %s\n", 
                  data_in_volts, data_in_amps, wifi_connected ? "Connected" : "Disconnected");
              
      // Write data to SD card files
      // Files are named "Amps YYYY-MM-DD.txt" and "Volts YYYY-MM-DD.txt"
      write_file(data_in_amps, count, "Amps ");
      write_file(data_in_volts, count, "Volts ");
              
      // Send data to MQTT if connected
      #if ENABLE_MQTT
      if (wifi_connected && mqtt.connected()) {
        publish_data_point(data_in_amps, data_in_volts, timestamp);
      } else if (wifi_connected && !mqtt.connected()) {
        // Try to reconnect to MQTT if WiFi is connected but MQTT is not
        connect_mqtt();
      }
      // Keep MQTT client connection alive
      if (mqtt.connected()) {
        mqtt.loop();
      }
      #endif
              
      #if ENABLE_DISPLAY
      // Update the display with current readings
      // Determine current direction (charging or discharging)
      if (data_in_amps < 0) {
        direction = RIGHT; // Charging
      } else {
        direction = LEFT;  // Discharging
      }
                  
      // Update display interface with current values and direction
      interface(data_in_amps, direction);
      #endif
              
      count++;
    }
  }

  // Log completion of one minute cycle
  Serial.println("One minute cycle completed");
  Serial.printf("Date: %02d-%02d-%04d  Time: %02d:%02d:%02d\n", 
    now.day(), now.month(), now.year(), 
    now.hour(), now.minute(), now.second());
  
  // Reset counter for the next minute
  count = 0;
#endif
}

// ===== DATA COLLECTION FUNCTIONS =====

/**
 * Read current from ADS1115 differential input
 * @return Current in Amperes
 */
float get_adc_data_in_A() {
  int16_t results;
  
  // Conversion factor for ADS1115 with GAIN_SIXTEEN (0.0078125 mV per bit)
  const double Ampmultiplier = 0.0078125;
  
  // Read differential voltage across shunt
  results = ads.readADC_Differential_0_1();
  
  // Convert to amperes based on shunt rating (100A/75mV)
  float amps = float((results * Ampmultiplier * (SHUNT_SIZE / 75.000)));
  return amps;
}

/**
 * Read voltage from ADS1115 single-ended input
 * @return Voltage in Volts
 */
float get_adc_data_in_V() {
  int16_t results;
  
  // Conversion factor for voltage divider
  const double Voltmultiplier = 0.0002696;
  
  // Read from single-ended input (A2)
  results = ads.readADC_SingleEnded(2);
  
  // Convert to volts (includes voltage divider factor of 2)
  float volts_ = float(2 * Voltmultiplier * results) + VOLTAGE_OFFSET;
  float volts = round(volts_*10)/10;
  return volts;
}

/**
 * Write data to a file on the SD card
 * @param data The data value to write
 * @param count Current sample count (0-59)
 * @param filnamn Prefix for the filename
 */
void write_file(float data, int count, String filnamn) {
  // Get current date/time
  DateTime time = rtc.now();
   
  // Create filename with date: "Prefix YYYY-MM-DD.txt"
  String date = time.timestamp(DateTime::TIMESTAMP_DATE);
  String txt = ".txt";
  String filename_merged = filnamn + date + txt;
  
  // Convert to char array for SD library
  char filename[filename_merged.length() + 1];
  filename_merged.toCharArray(filename, sizeof(filename));
  
  // Debug output with filename and count
  Serial.print("Writing to file: ");
  Serial.print(filename);
  Serial.print(" | Value: ");
  Serial.print(data);
  Serial.print(" | Count: ");
  Serial.println(count);
  
  // Open file for append - if fails, halt with error
  if (!file.open(filename, O_RDWR | O_CREAT | O_AT_END)) {
    Serial.print("ERROR: Failed to open file for writing: ");
    Serial.println(filename);
    
    // Try again once
    if (!sd.begin(chipSelect, SPI_HALF_SPEED)) {
      Serial.println("ERROR: SD card reinit failed!");
      return;
    }
    
    // Second attempt after reinitializing
    if (!file.open(filename, O_RDWR | O_CREAT | O_AT_END)) {
      Serial.println("ERROR: Still failed to open file after SD reinit!");
      return;
    }
    
    Serial.println("File opened successfully after SD reinit");
  }
  
  // Start a new line with timestamp at the beginning of each minute
  if (count == 0) {
    // Use time format HH:MM:SS
    char timestamp[9]; // HH:MM:SS + null terminator
    sprintf(timestamp, "%02d:%02d:%02d", 
            time.hour(), time.minute(), time.second());
    
    file.println(); // Start on a new line
    file.print(timestamp);
    file.print(" --> ");
    
    Serial.print("Wrote timestamp: ");
    Serial.println(timestamp);
  }

  // Write data value with comma separator (except for last value)
  file.print(data);
  if (count < 59) {
    file.print(", ");
  } else {
    // End the line on the last sample of the minute
    file.println(); // Important - ensures the line ends properly
  }

  // Close the file to ensure data is saved
  file.close();
  
  // Verify file exists with size check (only do once per minute to save performance)
  if (count == 59) {
    SdFile checkFile;
    if (checkFile.open(filename, O_READ)) {
      Serial.print("File size: ");
      Serial.print(checkFile.fileSize());
      Serial.println(" bytes");
      checkFile.close();
    }
  }
}

/**
 * Check WiFi connection and attempt to reconnect if needed
 */
void check_wifi_connection() {
  last_wifi_attempt = millis();
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to WiFi... ");
    
    // Make sure we're in station mode
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    // Wait up to 10 seconds for connection
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECT_TIMEOUT) {
      delay(500);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      wifi_connected = true;
      Serial.println("Connected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      
      // Connect to MQTT broker if enabled
      #if ENABLE_MQTT
      connect_mqtt();
      #endif
    } else {
      wifi_connected = false;
      Serial.println("Failed. Will retry later.");
      WiFi.disconnect(true);
    }
  } else {
    wifi_connected = true;
  }
}

/**
 * Setup web server routes for file browsing and download
 */
void setup_web_server() {
  // Root page - serve file browser directly instead of redirecting
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>Battery Monitor Data Files</title>";
    html += "<style>";
    html += "body{font-family:Arial,sans-serif;margin:20px;background-color:#f5f5f5;}";
    html += "h1,h2{color:#333;text-align:center;}";
    html += "ul{list-style-type:none;padding:0;max-width:800px;margin:0 auto;}";
    html += "li{margin:10px 0;padding:15px;border-radius:8px;background-color:#fff;box-shadow:0 2px 4px rgba(0,0,0,0.1);display:flex;justify-content:space-between;align-items:center;}";
    html += "a{text-decoration:none;color:#0066cc;display:block;}";
    html += "a:hover{text-decoration:underline;}";
    html += ".file-actions{display:flex;gap:10px;}";
    html += ".btn{border:none;border-radius:4px;padding:6px 12px;cursor:pointer;font-weight:bold;text-align:center;text-decoration:none;}";
    html += ".btn-view{background-color:#28a745;color:white;}";
    html += ".btn-delete{background-color:#dc3545;color:white;}";
    html += ".btn-update{background-color:#007bff;color:white;margin:0 auto;display:block;width:200px;}";
    html += ".btn:hover{opacity:0.9;}";
    html += ".header{background-color:#343a40;color:white;padding:20px;border-radius:8px;margin-bottom:20px;}";
    html += "hr{border:0;height:1px;background-color:#ddd;margin:20px 0;}";
    html += ".footer{text-align:center;padding:10px;color:#666;font-size:0.9em;}";
    html += ".path-nav{background-color:#e9ecef;padding:10px;border-radius:6px;margin-bottom:15px;text-align:center;}";
    html += ".confirm-modal{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background-color:rgba(0,0,0,0.5);align-items:center;justify-content:center;}";
    html += ".modal-content{background-color:white;padding:20px;border-radius:8px;max-width:400px;text-align:center;}";
    html += ".modal-buttons{display:flex;justify-content:center;gap:10px;margin-top:20px;}";
    html += "</style>";
    
    // Add JavaScript for delete confirmation
    html += "<script>";
    html += "function confirmDelete(filename) {";
    html += "  document.getElementById('file-to-delete').textContent = filename;";
    html += "  document.getElementById('delete-form').action = '/delete';"; // Changed to use form input instead of URL parameter
    html += "  document.getElementById('file-input').value = filename;";
    html += "  document.getElementById('delete-modal').style.display = 'flex';";
    html += "}";
    html += "function closeModal() {";
    html += "  document.getElementById('delete-modal').style.display = 'none';";
    html += "}";
    html += "</script>";
    
    html += "</head><body>";
    
    html += "<div class='header'>";
    html += "<h1>Battery Management System</h1>";
    html += "<p>Data File Browser</p>";
    html += "</div>";
    
    // Get directory path from query parameter or use root
    String path = "/";
    if(request->hasParam("dir")) {
      path = request->getParam("dir")->value();
    }
    
    html += "<div class='path-nav'>";
    html += "<a href='/?dir=/'>[Root Directory]</a> | Current Path: " + path;
    html += "</div>";
    
    html += "<h2>Files</h2>";
    html += list_files(path);
    
    html += "<hr>";
    html += "<div class='footer'>";
    html += "<a href='/update' class='btn btn-update'>OTA Update</a>";
    html += "<p>ESP32 Battery Management System | IP: " + WiFi.localIP().toString() + "</p>";
    html += "</div>";
    
    // Add delete confirmation modal
    html += "<div id='delete-modal' class='confirm-modal'>";
    html += "<div class='modal-content'>";
    html += "<h3>Confirm Delete</h3>";
    html += "<p>Are you sure you want to delete the file:</p>";
    html += "<p><strong id='file-to-delete'></strong>?</p>";
    html += "<p>This action cannot be undone.</p>";
    html += "<div class='modal-buttons'>";
    html += "<form id='delete-form' method='post' action='/delete'>";
    html += "<input type='hidden' id='file-input' name='file' value=''>";
    html += "<button type='submit' class='btn btn-delete'>Delete</button>";
    html += "</form>";
    html += "<button onclick='closeModal()' class='btn'>Cancel</button>";
    html += "</div></div></div>";
    
    html += "</body></html>";
    
    request->send(200, "text/html", html);
  });

  // Old path /getdata now redirects to root
  server.on("/getdata", HTTP_GET, [](AsyncWebServerRequest *request){
    // Preserve any query parameters
    String queryString = "";
    if(request->hasParam("dir")) {
      queryString = "?dir=" + request->getParam("dir")->value();
    }
    request->redirect("/" + queryString);
  });

  // File view/download handler
  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("file")) {
      String filepath = request->getParam("file")->value();
      
      // Check if file exists
      SdFile downloadFile;
      if(downloadFile.open(filepath.c_str(), O_READ)) {
        downloadFile.close();  // Close it immediately so that AsyncWebServer can handle it
        
        // Get file name from path
        String filename = filepath;
        int lastSlash = filepath.lastIndexOf('/');
        if(lastSlash >= 0) {
          filename = filepath.substring(lastSlash + 1);
        }
        
        // Send file - read the file contents and send them
        SdFile dataFile;
        if (dataFile.open(filepath.c_str(), O_READ)) {
          // Read file and send its contents
          String fileContent = "";
          char buffer[64];
          int bytesRead;
          
          while ((bytesRead = dataFile.read(buffer, sizeof(buffer))) > 0) {
            // Add read bytes to the string
            for (int i = 0; i < bytesRead; i++) {
              fileContent += buffer[i];
            }
          }
          dataFile.close();
          
          // Set appropriate content type based on file extension
          String contentType = "text/plain";
          if (filename.endsWith(".txt")) contentType = "text/plain";
          else if (filename.endsWith(".csv")) contentType = "text/csv";
          else if (filename.endsWith(".json")) contentType = "application/json";
          
          request->send(200, contentType, fileContent);
        } else {
          request->send(404, "text/plain", "File not found");
        }
      } else {
        request->send(404, "text/plain", "File not found");
      }
    } else {
      request->send(400, "text/plain", "File parameter missing");
    }
  });
  
  // File deletion handler - change from GET to POST
  server.on("/delete", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("file", true)) { // true indicates POST parameter
      String filepath = request->getParam("file", true)->value();
      
      // Check if file exists
      SdFile deleteFile;
      if(deleteFile.open(filepath.c_str(), O_READ)) {
        deleteFile.close();  // Close it before attempting to delete
        
        // Get file name from path
        String filename = filepath;
        int lastSlash = filepath.lastIndexOf('/');
        if(lastSlash >= 0) {
          filename = filepath.substring(lastSlash + 1);
        }
        
        // Get directory path for redirect
        String path = "/";
        if(lastSlash >= 0) {
          path = filepath.substring(0, lastSlash + 1);
        }
        
        // Delete the file
        if(sd.remove(filepath.c_str())) {
          // Redirect to the directory view with success message
          request->redirect("/?dir=" + path + "&msg=File+" + filename + "+deleted+successfully");
        } else {
          // Redirect with error message
          request->redirect("/?dir=" + path + "&error=Failed+to+delete+" + filename);
        }
      } else {
        request->send(404, "text/plain", "File not found");
      }
    } else {
      request->send(400, "text/plain", "File parameter missing");
    }
  });

  // Setup ElegantOTA
  ElegantOTA.begin(&server);
  
  // Start the server
  server.begin();
  Serial.println("Web server started");
}

/**
 * List all files in the given directory
 * @param path Directory path on SD card
 * @return HTML formatted file list
 */
String list_files(String path) {
  String output = "<ul>";
  
  // End path with a slash if missing
  if(!path.endsWith("/")) path += "/";
  
  // Try to open the directory
  SdFile dir;
  if(!dir.open(path.c_str(), O_READ)) {
    return "<div style='text-align:center;padding:20px;background-color:#f8d7da;color:#721c24;border-radius:8px;'>Failed to open directory</div>";
  }
  
  // Add parent directory link if not in root
  if(path != "/") {
    // Get parent directory path
    String parentPath = path;
    // Remove trailing slash
    parentPath.remove(parentPath.length() - 1);
    // Find last slash
    int lastSlash = parentPath.lastIndexOf('/');
    if(lastSlash > 0) {
      parentPath = parentPath.substring(0, lastSlash + 1);
    } else {
      parentPath = "/";
    }
    
    output += "<li>";
    output += "<div><a href='/?dir=" + parentPath + "'><strong>[Parent Directory]</strong></a></div>";
    output += "<div class='file-actions'></div>"; // Empty actions for parent dir
    output += "</li>";
  }
  
  // Count of files and directories
  int fileCount = 0;
  int dirCount = 0;
  
  // Loop through files and directories
  SdFile entry;
  while(entry.openNext(&dir, O_READ)) {
    char fileName[64];
    entry.getName(fileName, sizeof(fileName));
    String name = String(fileName);
    
    if(entry.isDir()) {
      // Directory entry
      dirCount++;
      output += "<li>";
      output += "<div><a href='/?dir=" + path + name + "'><strong>[DIR] " + name + "</strong></a></div>";
      output += "<div class='file-actions'></div>"; // Empty actions for directories
      output += "</li>";
    } else {
      // File entry with download link and delete button
      fileCount++;
      size_t fileSize = entry.fileSize();
      String sizeStr = get_file_size(fileSize);
      
      output += "<li>";
      output += "<div>" + name + " <span style='color:#666;font-size:0.9em;'>(" + sizeStr + ")</span></div>";
      output += "<div class='file-actions'>";
      output += "<a href='/download?file=" + path + name + "' class='btn btn-view'>View</a>"; // Changed from Download to View
      output += "<a href='javascript:void(0)' onclick='confirmDelete(\"" + path + name + "\")' class='btn btn-delete'>Delete</a>";
      output += "</div>";
      output += "</li>";
    }
    
    entry.close();
  }
  
  dir.close();
  output += "</ul>";
  
  // Add summary of files and directories
  if (fileCount == 0 && dirCount == 0) {
    output = "<div style='text-align:center;padding:20px;background-color:#fff3cd;color:#856404;border-radius:8px;margin-bottom:20px;'>Directory is empty</div>" + output;
  } else {
    String summary = "<div style='text-align:center;margin-bottom:15px;'>";
    summary += String(dirCount) + " " + (dirCount == 1 ? "directory" : "directories") + ", ";
    summary += String(fileCount) + " " + (fileCount == 1 ? "file" : "files");
    summary += "</div>";
    output = summary + output;
  }
  
  return output;
}

/**
 * Format file size in human readable format
 * @param bytes File size in bytes
 * @return Formatted size string (e.g. "1.2 MB")
 */
String get_file_size(size_t bytes) {
  if(bytes < 1024) {
    return String(bytes) + " B";
  } else if(bytes < 1048576) { // 1MB
    float kb = bytes / 1024.0;
    return String(kb, 1) + " KB";
  } else if(bytes < 1073741824) { // 1GB
    float mb = bytes / 1048576.0;
    return String(mb, 1) + " MB";
  } else { 
    float gb = bytes / 1073741824.0;
    return String(gb, 1) + " GB";
  }
}

#if ENABLE_MQTT
/**
 * Connect to MQTT broker
 * @return true if successfully connected, false otherwise
 */
bool connect_mqtt() {
  if (!wifi_connected) return false;
  
  // Loop until we're connected
  int attempts = 0;
  while (!mqtt.connected() && attempts < 3) {
    Serial.print("Attempting MQTT connection...");
    
    // Attempt to connect with last will message for status
    if (mqtt.connect(mqtt_client_id, mqtt_user, mqtt_pass, 
                    mqtt_topic_status, 0, false, "{\"status\":\"offline\"}")) {
      Serial.println("connected");
      
      // Once connected, publish an online status message
      String status_message = "{\"status\":\"online\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
      mqtt.publish(mqtt_topic_status, status_message.c_str(), true);
      return true;
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
      attempts++;
    }
  }
  
  return false;
}

/**
 * Publish a single data point to MQTT
 * @param current Current measurement in Amperes
 * @param voltage Voltage measurement in Volts
 * @param timestamp Timestamp of the measurement
 */
void publish_data_point(float current, float voltage, DateTime timestamp) {
  if (!mqtt.connected()) return;
  
  // Create JSON-like message with timestamp and values
  char buffer[150];
  sprintf(buffer, 
          "{\"timestamp\":\"%04d-%02d-%02d %02d:%02d:%02d\",\"current\":%.3f,\"voltage\":%.2f}", 
          timestamp.year(), timestamp.month(), timestamp.day(),
          timestamp.hour(), timestamp.minute(), timestamp.second(),
          current, voltage);
  
  // Publish current value to current topic
  int retries = 0;
  while (!mqtt.publish(mqtt_topic_current, buffer) && retries < 3) {
    Serial.println("Failed to publish current data, retrying...");
    delay(100);
    mqtt.loop();
    retries++;
  }
  
  // Publish voltage to voltage topic (use the same message for simplicity)
  retries = 0;
  while (!mqtt.publish(mqtt_topic_voltage, buffer) && retries < 3) {
    Serial.println("Failed to publish voltage data, retrying...");
    delay(100);
    mqtt.loop();
    retries++;
  }
  
  if (retries >= 3) {
    Serial.println("Failed to publish one or more data points");
  } else {
    Serial.println("Data published to MQTT successfully");
  }
  
  // Process any incoming messages
  mqtt.loop();
}
#endif

#if ENABLE_DISPLAY
/**
 * Updates the OLED display with current battery information
 * @param amps Current measurement in Amperes
 * @param dir Current direction (LEFT=discharge, RIGHT=charge)
 */
void interface(float amps, int dir) {
  // Clear the display
  display.clearDisplay();
  
  // Draw battery interface elements
  display.drawLine(41, 46, 45, 46, WHITE);
  display.drawLine(65, 46, 69, 46, WHITE);
  display.drawLine(71, 46, 75, 46, WHITE);
  display.drawLine(59, 46, 63, 46, WHITE);
  display.drawLine(53, 46, 57, 46, WHITE);
  display.drawLine(47, 46, 51, 46, WHITE);
  
  // Draw logos and icons
  display.drawBitmap(79, 16, image_volvo_bits, 50, 50, WHITE);
  display.drawBitmap(78, 30, image_Layer_18_bits, 51, 17, WHITE);
  display.drawBitmap(0, 31, image_battery__2__bits, 40, 30, WHITE);
  
  // Add WiFi status indicator
  display.setCursor(0, 0);
  display.print("WiFi:");
  display.print(wifi_connected ? "Connected" : "Disconnected");
  
  // Call animation function for arrow
  move_arrow(dir, amps);
  
  // Update display
  display.display();
}

/**
 * Animates an arrow indicating current direction
 * @param dir Direction (LEFT=discharge, RIGHT=charge)
 * @param amps Current measurement to display
 */
void move_arrow(int dir, float amps) {
  int x = 38;
  int x_slut = 69;

  if (dir == RIGHT) {
    // Discharging animation
    x = 38;
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(3, 20);
    display.setTextWrap(false);
    display.print("Discharging:");
    display.print(amps);
    display.print(" A");
    
    // Animate the arrow
    for (int i = 0; i < 3; i++) {  // Reduced animation cycles
      display.drawBitmap(x, 37, image_Pin_arrow_right_9x7_bits, 9, 7, WHITE);
      display.display();
      delay(10);
      display.drawBitmap(x, 37, image_Pin_arrow_right_9x7_bits, 9, 7, BLACK);
      x++;
      if (x == x_slut) x = 38;
    }
  } else if (dir == LEFT) {
    // Charging animation
    x = x_slut;
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(3, 20);
    display.setTextWrap(false);
    display.print("Charging:");
    display.print(amps);
    display.print(" A");
    
    // Animate the arrow
    for (int i = 0; i < 3; i++) {  // Reduced animation cycles
      display.drawBitmap(x, 37, image_Pin_arrow_left_9x7_bits, 9, 7, WHITE);
      display.display();
      delay(10);
      display.drawBitmap(x, 37, image_Pin_arrow_left_9x7_bits, 9, 7, BLACK);
      x--;
      if (x == 38) x = x_slut;
    }
  }
}
#endif


