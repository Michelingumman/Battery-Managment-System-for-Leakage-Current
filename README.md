<div align="center">
  <h1>🔋 ESP32 Battery Management System 🔌</h1>
  <p><em>A comprehensive solution for monitoring battery performance with wireless connectivity, data logging, and real-time analysis</em></p>

  <p>
    <img src="https://img.shields.io/badge/ESP32-Enabled-blue?style=flat-square&logo=espressif" alt="ESP32"/>
    <img src="https://img.shields.io/badge/MQTT-Connected-green?style=flat-square&logo=mqtt" alt="MQTT"/>
    <img src="https://img.shields.io/badge/OTA-Updates-orange?style=flat-square&logo=arduino" alt="OTA"/>
    <img src="https://img.shields.io/badge/Web-Interface-purple?style=flat-square&logo=html5" alt="Web"/>
  </p>
</div>

<div align="center">
  <table>
    <tr>
      <td width="70%">
        <img src="https://github.com/user-attachments/assets/de285fad-d931-4771-810e-c3d69a32fc3e" width="100%" alt="MQTT Dashboard"/>
        <p align="center"><strong>MQTT Dashboard</strong> - Real-time monitoring</p>
      </td>
      <td width="50%">
        <img src="https://github.com/user-attachments/assets/bb96bbee-dab2-4f67-a350-726b1b63d7a1" width="100%" alt="System Overview"/>
        <p align="center"><strong>Added Display</strong> - Intuative animation</p>
      </td>
    </tr>
  </table>
</div>

<p align="center">
  <img src="https://github.com/user-attachments/assets/5f308408-5da9-487b-b2f8-30014f531c52" width="90%" alt="OLED Display"/>
  <br>
  <em>On-device OLED display showing current battery status</em>
</p>

---

## ✨ Features

- **📊 High-Precision Measurements**: Uses ADS1115 16-bit ADC for accurate voltage and current readings
- **💾 Data Logging**: Automatically records measurements to SD card with timestamps
- **🌐 Web Interface**: Access and download logged data files via browser without removing SD card
- **📡 Real-Time MQTT**: Publishes data points to MQTT broker for remote monitoring
- **🔄 OTA Updates**: Update firmware wirelessly through web interface
- **📱 Optional Display**: OLED display support for direct status viewing (configurable)

## 🛠️ Hardware Requirements

<div align="center">
  <table>
    <tr>
      <td align="center"><b>Component</b></td>
      <td align="center"><b>Purpose</b></td>
    </tr>
    <tr>
      <td>ESP32 Development Board</td>
      <td>Main controller with WiFi capability</td>
    </tr>
    <tr>
      <td>ADS1115 16-bit ADC Module</td>
      <td>High-precision analog-to-digital conversion</td>
    </tr>
    <tr>
      <td>DS3231 RTC Module</td>
      <td>Accurate timestamping of measurements</td>
    </tr>
    <tr>
      <td>SD Card Module</td>
      <td>Data storage and logging</td>
    </tr>
    <tr>
      <td>Current Shunt (100A/75mV)</td>
      <td>Current measurement</td>
    </tr>
    <tr>
      <td>SSD1306 OLED Display (Optional)</td>
      <td>Real-time data visualization</td>
    </tr>
  </table>
</div>

## 📚 Software Dependencies

- **Arduino IDE** with ESP32 support
- **Libraries**:
  - `WiFi`, `AsyncTCP`, `ESPAsyncWebServer` - Network connectivity
  - `SdFat` - SD card file operations
  - `RTClib` - RTC module interface
  - `Adafruit_ADS1X15` - ADC interface
  - `PubSubClient` - MQTT functionality
  - `ElegantOTA` - Over-the-air updates
  - `Adafruit_GFX` and `Adafruit_SSD1306` - Display support (if enabled)

## 🚀 Setup

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

## 📖 Usage

### 📊 Data Collection
- The system automatically logs current and voltage data to SD card
- Data files are named `Amps YYYY-MM-DD.txt` and `Volts YYYY-MM-DD.txt`
- Each line contains timestamped measurements in the format `HH:MM:SS --> value1, value2, ...`

### 🌐 Web Interface
1. Connect to the same WiFi network as ESP32
2. Navigate to `http://<ESP32_IP_ADDRESS>/getdata`
3. Browse and download data files directly through your browser
4. Access OTA update page at `http://<ESP32_IP_ADDRESS>/update`

### 📡 MQTT Monitoring
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

## ⚙️ Configuration

Key parameters can be adjusted in the code:
- WiFi reconnection interval
- MQTT topic names
- Display settings
- SD card pins

## 📄 License

MIT License

## 🙏 Acknowledgments

- Thanks to all libraries and their contributors
- Inspired by various battery monitoring projects

