# ESP32 Battery Management System

A comprehensive ESP32-based system for monitoring battery performance with wireless connectivity, data logging, and real-time analysis.
![image](https://github.com/user-attachments/assets/de285fad-d931-4771-810e-c3d69a32fc3e)

![System Overview](https://github.com/user-attachments/assets/bb96bbee-dab2-4f67-a350-726b1b63d7a1)
![image](https://github.com/user-attachments/assets/5f308408-5da9-487b-b2f8-30014f531c52)


## Features

- **High-Precision Measurements**: Uses ADS1115 16-bit ADC for accurate voltage and current readings
- **Data Logging**: Automatically records measurements to SD card with timestamps
- **Web Interface**: Access and download logged data files via browser without removing SD card
- **Real-Time MQTT**: Publishes data points to MQTT broker for remote monitoring
- **OTA Updates**: Update firmware wirelessly through web interface
- **Optional Display**: OLED display support for direct status viewing (configurable)

## Hardware Requirements

- ESP32 development board
- ADS1115 16-bit ADC module
- DS3231 RTC module
- SD card module
- Current shunt (100A/75mV)
- Optional: SSD1306 OLED display

## Software Dependencies

- Arduino IDE with ESP32 support
- Libraries:
  - WiFi, AsyncTCP, ESPAsyncWebServer
  - SdFat
  - RTClib
  - Adafruit_ADS1X15
  - PubSubClient (for MQTT)
  - ElegantOTA
  - Adafruit_GFX and Adafruit_SSD1306 (if using display)

## Setup

1. **Create a `secrets.h` file** with your credentials:
   ```cpp
   #define WIFI_SSID "your_wifi_ssid"
   #define WIFI_PASS "your_wifi_password"
   #define MQTT_SERVER "your_mqtt_broker_address"
   #define MQTT_PORT 1883
   #define MQTT_USER "your_mqtt_username"
   #define MQTT_PASS "your_mqtt_password"
   ```

2. **Connect hardware components** according to pinout defined in code
3. **Upload the firmware** to ESP32

## Usage

### Data Collection
- The system automatically logs current and voltage data to SD card
- Data files are named `Amps YYYY-MM-DD.txt` and `Volts YYYY-MM-DD.txt`
- Each line contains timestamped measurements in the format `HH:MM:SS --> value1, value2, ...`

### Web Interface
1. Connect to the same WiFi network as ESP32
2. Navigate to `http://<ESP32_IP_ADDRESS>/getdata`
3. Browse and download data files directly through your browser
4. Access OTA update page at `http://<ESP32_IP_ADDRESS>/update`

### MQTT Monitoring
The system publishes to three MQTT topics:

1. `battery/data/current` - Real-time current measurements
2. `battery/data/voltage` - Real-time voltage measurements 
3. `battery/status` - Connection status with device IP address

Message format (JSON):
```json
{
  "timestamp": "2024-06-03 14:35:22",
  "current": 0.123,
  "voltage": 12.45
}
```

## Configuration

Key parameters can be adjusted in the code:
- WiFi reconnection interval
- MQTT topic names
- Display settings
- SD card pins

## License

MIT License

## Acknowledgments

- Thanks to all libraries and their contributors
- Inspired by various battery monitoring projects

