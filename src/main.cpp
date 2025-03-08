/**
 * Battery Management System for Leakage Current
 * 
 * This ESP32-based system monitors battery voltage and current,
 * storing timestamped readings to SD card files for later analysis.
 * When parked and connected to WiFi, data is uploaded to an MQTT broker.
 * 
 * Features:
 * - High-precision ADC measurements for voltage and current
 * - RTC-synchronized timestamps
 * - SD card data logging
 * - MQTT data upload when parked and connected to WiFi
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
#include <ElegantOTA.h>     // OTA update functionality
#include <ESPAsyncWebServer.h>


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
const char* mqtt_topic_prefix = "battery/data/";
#endif

// Timing settings
const unsigned long WIFI_RETRY_INTERVAL = 30000;  // Try to reconnect WiFi every 30 seconds
const unsigned long MQTT_UPLOAD_INTERVAL = 60000; // Check for data to upload every minute
const float PARKED_CURRENT_THRESHOLD = 0.5;       // Current below this is considered "parked" (A)
const int PARKED_CONFIRMATION_TIME = 300;         // Time (seconds) to confirm car is parked

// ===== OBJECT INITIALIZATION =====
Adafruit_ADS1115 ads;       // 16-bit ADC object
RTC_DS3231 rtc;             // Real-time clock object
SdFat sd;                   // SD card filesystem
SdFile file;                // File object for writing
AsyncWebServer server(80);  // Web server for OTA updates

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
unsigned long last_mqtt_upload = 0;
bool is_parked = false;
int parked_confirmation_counter = 0;
String uploaded_files[50];  // Keep track of uploaded files
int num_uploaded_files = 0;

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
#if ENABLE_MQTT
void check_wifi_connection();            // Check and attempt to reconnect WiFi
void connect_mqtt();                     // Connect to MQTT broker
void check_and_upload_data();            // Upload stored data when connected
bool upload_file_to_mqtt(String filepath); // Upload a specific file
bool is_file_uploaded(String filepath);  // Check if file was already uploaded
#endif

// Parked state detection
void check_parked_state(float current_reading);

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
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting to WiFi");
  // Try to connect for up to 10 seconds - if not connected, we'll try again later when parked
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifi_connected = true;
    Serial.println("");
    Serial.print("Connected to WiFi: ");
    Serial.println(WIFI_SSID);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed. Will retry when parked.");
    WiFi.disconnect(true);
    wifi_connected = false;
  }

  // Setup OTA update server (only works when WiFi is connected)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Battery Management System - OTA Update Server");
  });

  ElegantOTA.begin(&server);    // Start ElegantOTA
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  if (wifi_connected) {
    server.begin();
    Serial.println("HTTP server started");
  }

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

  // Initialize ADS1115 ADC
  Serial.println("Initializing ADC...");
  // Set ADC gain for highest precision in our voltage range
  // GAIN_SIXTEEN: +/- 0.256V range with 0.0078125mV per bit
  ads.setGain(GAIN_SIXTEEN);

  if (!ads.begin()) {
    Serial.println("ERROR: Failed to initialize ADS1115!");
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
  
  // Collect and log data every minute (60 samples at ~1 second intervals)
  while (count < 60) {
    // Read voltage and current values
    float data_in_amps = get_adc_data_in_A();
    float data_in_volts = get_adc_data_in_V();
    
    // Check if car is parked based on current reading
    check_parked_state(data_in_amps);
    
    // If parked, check WiFi and MQTT connections
    if (is_parked) {
      #if ENABLE_MQTT
      // Try to reconnect WiFi if needed
      if (millis() - last_wifi_attempt > WIFI_RETRY_INTERVAL) {
        check_wifi_connection();
      }
      
      // Upload data if connected
      if (wifi_connected && mqtt.connected() && 
          (millis() - last_mqtt_upload > MQTT_UPLOAD_INTERVAL)) {
        check_and_upload_data();
        last_mqtt_upload = millis();
      }
      #endif
    }
    
    // Display readings on serial monitor
    Serial.printf("Volts: %.2fV | Amps: %.2fA | Parked: %s\n", 
                 data_in_volts, data_in_amps, is_parked ? "Yes" : "No");
    
    // Write data to SD card files
    // Files are named "Amps YYYY-MM-DD.txt" and "Volts YYYY-MM-DD.txt"
    write_file(data_in_amps, count, "Amps ");
    write_file(data_in_volts, count, "Volts ");
    
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
    
    // Keep MQTT connection alive if connected
    #if ENABLE_MQTT
    if (mqtt.connected()) {
      mqtt.loop();
    }
    #endif
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
 * Check if the car is parked based on current readings
 * We define "parked" as current below a threshold for a certain period
 */
void check_parked_state(float current_reading) {
  // Check if current is below the threshold indicating car is not being driven
  if (abs(current_reading) < PARKED_CURRENT_THRESHOLD) {
    parked_confirmation_counter++;
    if (parked_confirmation_counter >= PARKED_CONFIRMATION_TIME && !is_parked) {
      is_parked = true;
      Serial.println("Car is now PARKED. Will attempt data upload if WiFi available.");
    }
  } else {
    // Reset counter if current goes above threshold
    parked_confirmation_counter = 0;
    if (is_parked) {
      is_parked = false;
      Serial.println("Car is no longer parked. Data upload suspended.");
    }
  }
}

#if ENABLE_MQTT
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
    for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
      delay(1000);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      wifi_connected = true;
      Serial.println("Connected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      
      // Start web server for OTA updates now that we're connected
      if (!server.started()) {
        server.begin();
        Serial.println("HTTP server started");
      }
      
      // Connect to MQTT broker
      connect_mqtt();
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
 * Connect to MQTT broker
 */
void connect_mqtt() {
  if (!wifi_connected) return;
  
  // Loop until we're connected
  int attempts = 0;
  while (!mqtt.connected() && attempts < 3) {
    Serial.print("Attempting MQTT connection...");
    
    // Attempt to connect
    if (mqtt.connect("ESP32Client", mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      
      // Once connected, publish an announcement
      String announce_topic = String(mqtt_topic_prefix) + "status";
      mqtt.publish(announce_topic.c_str(), "ESP32 Battery Monitor connected");
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
      attempts++;
    }
  }
}

/**
 * Check SD card for data files and upload them to MQTT broker if connected
 */
void check_and_upload_data() {
  if (!wifi_connected || !mqtt.connected()) return;
  
  Serial.println("Checking for data to upload...");
  
  // Open the SD card root directory
  SdFile root;
  if (!root.open("/")) {
    Serial.println("ERROR: Failed to open root directory");
    return;
  }
  
  // Create a buffer for file objects
  SdFile file;
  
  // Loop through all files
  while (file.openNext(&root, O_READ)) {
    char filename[64];
    file.getName(filename, sizeof(filename));
    file.close();
    
    // Check if it's a data file (Amps or Volts) and hasn't been uploaded
    String filenameStr = String(filename);
    if ((filenameStr.startsWith("Amps") || filenameStr.startsWith("Volts")) && 
        filenameStr.endsWith(".txt") && 
        !is_file_uploaded(filenameStr)) {
      
      Serial.print("Found data file to upload: ");
      Serial.println(filenameStr);
      
      // Upload the file
      if (upload_file_to_mqtt(filenameStr)) {
        // Add to uploaded files list
        if (num_uploaded_files < 50) {
          uploaded_files[num_uploaded_files++] = filenameStr;
          Serial.println("File uploaded successfully");
        } else {
          Serial.println("Warning: Uploaded files list full. Oldest entries may be re-uploaded.");
          // Shift array to make room
          for (int i = 0; i < 49; i++) {
            uploaded_files[i] = uploaded_files[i+1];
          }
          uploaded_files[49] = filenameStr;
        }
      } else {
        Serial.println("Failed to upload file");
      }
    }
  }
  
  root.close();
}

/**
 * Upload a file to MQTT broker
 * @param filepath Path to the file to upload
 * @return true if successful, false otherwise
 */
bool upload_file_to_mqtt(String filepath) {
  // Open the file
  SdFile dataFile;
  if (!dataFile.open(filepath.c_str(), O_READ)) {
    Serial.println("ERROR: Failed to open data file");
    return false;
  }
  
  // Determine the topic based on file type
  String topic;
  if (filepath.startsWith("Amps")) {
    topic = String(mqtt_topic_prefix) + "current";
  } else if (filepath.startsWith("Volts")) {
    topic = String(mqtt_topic_prefix) + "voltage";
  } else {
    topic = String(mqtt_topic_prefix) + "other";
  }
  
  // Extract date from filename to include in message
  String date = "";
  int datePos = filepath.indexOf("20"); // Find year (20xx)
  if (datePos > 0) {
    date = filepath.substring(datePos, datePos + 10); // YYYY-MM-DD
  }
  
  // Create a buffer for reading the file
  const int bufferSize = 128;
  char buffer[bufferSize];
  
  // Read and publish the file line by line
  int lineCount = 0;
  int byteCount = 0;
  bool success = true;
  
  while (dataFile.available()) {
    int bytesRead = dataFile.fgets(buffer, bufferSize);
    if (bytesRead <= 0) break;
    
    // Create message with metadata
    String message = String(date) + "|" + String(lineCount) + "|" + String(buffer);
    
    // Publish to MQTT (with retry)
    int retries = 0;
    while (!mqtt.publish(topic.c_str(), message.c_str()) && retries < 3) {
      Serial.println("Failed to publish, retrying...");
      delay(100);
      mqtt.loop();
      retries++;
    }
    
    if (retries >= 3) {
      success = false;
      break;
    }
    
    lineCount++;
    byteCount += bytesRead;
    
    // Process MQTT messages to keep connection alive
    mqtt.loop();
  }
  
  dataFile.close();
  
  // Publish a summary message with file metadata
  String summaryTopic = String(mqtt_topic_prefix) + "summary";
  String summaryMsg = String(date) + "|" + filepath + "|lines:" + 
                      String(lineCount) + "|bytes:" + String(byteCount);
  mqtt.publish(summaryTopic.c_str(), summaryMsg.c_str());
  
  return success;
}

/**
 * Check if a file has already been uploaded
 * @param filepath Path of the file to check
 * @return true if already uploaded, false otherwise
 */
bool is_file_uploaded(String filepath) {
  for (int i = 0; i < num_uploaded_files; i++) {
    if (uploaded_files[i] == filepath) {
      return true;
    }
  }
  return false;
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
  
  // Add parked status indicator
  display.setCursor(0, 10);
  display.print("Parked:");
  display.print(is_parked ? "Yes" : "No");
  
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
    for (int i = 0; i < 3; i++) {  // Reduced animation cycles when parked
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
    for (int i = 0; i < 3; i++) {  // Reduced animation cycles when parked
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


