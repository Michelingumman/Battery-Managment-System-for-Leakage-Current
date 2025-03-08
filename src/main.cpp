/**
 * Battery Management System for Leakage Current
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

// ===== FUNCTION DECLARATIONS =====
// OTA callback functions
void onOTAStart();
void onOTAProgress(size_t current, size_t final);
void onOTAEnd(bool success);

// Data collection and storage functions
float get_adc_data_in_A();    // Get current reading in Amperes
float get_adc_data_in_V();    // Get voltage reading in Volts
void write_file(float data, int count, String filename); // Log data to SD card

// WiFi and MQTT functions
void check_wifi_connection();            // Check and attempt to reconnect WiFi

#if ENABLE_MQTT
bool connect_mqtt();                     // Connect to MQTT broker
void publish_data_point(float current, float voltage, DateTime timestamp); // Publish single data point
#endif

// Web server functions
void setup_web_server();                 // Setup web server routes
String list_files(String path);          // List all files in directory
String get_file_size(String filename);   // Get file size as string

// Display functions
#if ENABLE_DISPLAY
void interface(float amps, int dir);  // Update display interface
void move_arrow(int dir, float amps); // Animate the direction arrow
#endif

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

// ===== SETUP FUNCTION =====
void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("\n=== Battery Management System ===");
  Serial.println("Initializing...");

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
  
  // Set RTC to compilation time
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  DateTime time = rtc.now();
  Serial.print("Current time: ");
  Serial.printf("%02d:%02d:%02d\n", time.hour(), time.minute(), time.second());

  // Initialize SD card
  Serial.println("Initializing SD card...");
  if (!sd.begin(chipSelect, SPI_HALF_SPEED)) {
    Serial.println("ERROR: SD card initialization failed!");
    sd.initErrorHalt();
  }
  Serial.println("SD card initialized successfully");

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
}

// ===== MAIN LOOP =====
void loop() {
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
    
    // Wait approximately 1 second before next sample
    delay(980); // Adjusted for code execution time
    count++;
  }

  // Log completion of one minute cycle
  Serial.println("One minute cycle completed");
  Serial.printf("Date: %02d-%02d-%04d  Time: %02d:%02d:%02d\n", 
    now.day(), now.month(), now.year(), 
    now.hour(), now.minute(), now.second());
  
  // Reset counter for the next minute
  count = 0;
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
  float volts = float(2 * Voltmultiplier * results);
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
  
  // Open file for append
  if (!file.open(filename, O_RDWR | O_CREAT | O_AT_END)) {
    sd.errorHalt("ERROR: Failed to open file for writing");
  }
  
  // Start a new line with timestamp at the beginning of each minute
  if (count == 0) {
    file.println();
    file.print(time.timestamp(DateTime::TIMESTAMP_TIME));
    file.print(" --> ");
  }
  
  // Write data value with comma separator (except for last value)
  if (count < 59) {
    file.print(data);
    file.print(", ");
  } else {
    // End the line on the last sample of the minute
    file.print(data);
    file.println();
  }
  
  // Close the file to ensure data is saved
  file.close();
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
  // Root page - redirects to getdata
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("/getdata");
  });

  // Data browser page
  server.on("/getdata", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>Battery Monitor Data Files</title>";
    html += "<style>body{font-family:Arial,sans-serif;margin:20px;}h1{color:#333;}ul{list-style-type:none;padding:0;}";
    html += "li{margin:10px 0;padding:10px;border-radius:4px;background-color:#f2f2f2;}";
    html += "a{text-decoration:none;color:#0066cc;display:block;}";
    html += "a:hover{text-decoration:underline;}</style></head><body>";
    html += "<h1>Battery Monitor Data Files</h1>";
    
    // Add root directory link
    html += "<p><a href='/getdata?dir=/'>[Root Directory]</a></p>";
    
    // Get directory path from query parameter or use root
    String path = "/";
    if(request->hasParam("dir")) {
      path = request->getParam("dir")->value();
    }
    
    html += "<h2>Directory: " + path + "</h2>";
    html += list_files(path);
    html += "<hr><p><a href='/update'>OTA Update</a></p>";
    html += "</body></html>";
    
    request->send(200, "text/html", html);
  });

  // File download handler
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
    return "<p>Failed to open directory</p>";
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
    
    output += "<li><a href='/getdata?dir=" + parentPath + "'>[Parent Directory]</a></li>";
  }
  
  // Loop through files and directories
  SdFile entry;
  while(entry.openNext(&dir, O_READ)) {
    char fileName[64];
    entry.getName(fileName, sizeof(fileName));
    String name = String(fileName);
    
    if(entry.isDir()) {
      // Directory entry
      output += "<li><a href='/getdata?dir=" + path + name + "'>[DIR] " + name + "</a></li>";
    } else {
      // File entry with download link and size
      // String size = get_file_size(entry.fileSize());
      String size = "1mb";
      output += "<li><a href='/download?file=" + path + name + "'>" + name + " (" + size + ")</a></li>";
    }
    
    entry.close();
  }
  
  dir.close();
  output += "</ul>";
  
  return output;
}

/**
 * Format file size in human readable format
 * @param bytes File size in bytes
 * @return Formatted size string (e.g. "1.2 MB")
 */
String get_file_size(size_t bytes) {
  if(bytes < 1024) return String(bytes) + " B";
  else if(bytes < 1048576) return String(bytes / 1024.0, 1) + " KB";
  else if(bytes < 1073741824) return String(bytes / 1048576.0, 1) + " MB";
  else return String(bytes / 1073741824.0, 1) + " GB";
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
      mqtt.publish(mqtt_topic_status, "{\"status\":\"online\",\"ip\":\"" + WiFi.localIP().toString() + "\"}", true);
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


