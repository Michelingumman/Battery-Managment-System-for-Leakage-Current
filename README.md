# Battery Management System for Leakage Current
![image](https://github.com/user-attachments/assets/bb96bbee-dab2-4f67-a350-726b1b63d7a1)

A comprehensive system for monitoring, analyzing, and visualizing battery charging and discharging patterns, with a focus on leakage current measurement. This project combines ESP32-based data acquisition with Python-based data visualization.

![image](https://github.com/user-attachments/assets/3e27d749-df2c-4710-be02-5afdb6bcb3d6)
![image](https://github.com/user-attachments/assets/0eef07d9-9b81-4e27-b7ef-9badf6db41ec)

## Overview

This project provides tools to collect, process, and visualize battery data, helping to understand battery performance, discharge patterns, and capacity utilization. The system works with time-series data of voltage and current measurements to create detailed performance visualizations.

## System Architecture

The system consists of two main components:

1. **ESP32-based Data Acquisition System**: Collects voltage and current data using precision ADCs
2. **Python-based Visualization Tool**: Processes and visualizes the collected data

## Features

### Hardware Capabilities (ESP32)
- **Precision Measurement**: Uses high-resolution ADC for accurate current and voltage readings
- **Real-time Data Collection**: Programmable sampling rate via timer interrupts
- **Data Storage**: Writes timestamped data to separate files for voltage and current
- **Battery Connection Monitoring**: Detects and logs charging and discharging states


### Visualization Capabilities (Python)
- **Advanced Visualization**: Creates multi-axis plots showing:
  - Current (A) over time
  - Voltage (V) over time
  - Cumulative discharge capacity (Ah)


## ESP32 Implementation

### Hardware Requirements
- ESP32 development board
- Precision voltage divider for battery voltage measurement
- Current sensor (Shunt resistor or similar)
- microSD card module for data storage
- Real-time clock module (optional for precise timestamping)

### Libraries Used
- **SD.h**: File system operations on SD card
- **SPI.h**: Communication with SD card module
- **Wire.h**: I2C communication for optional peripherals
- **ESP32Time.h**: Real-time clock management
- **Arduino.h**: Core Arduino functionality

### Key Functions

```cpp
// Get current measurement from ADC and convert to Amperes
float get_adc_data_in_A() {
    // Read from ADC and apply calibration factors
    // Returns current in Amperes
}

// Get voltage measurement from ADC and convert to Volts
float get_adc_data_in_V() {
    // Read from ADC and apply voltage divider formula
    // Returns voltage in Volts
}

// Write data to file with timestamp
void write_file(float data, int count, String filename) {
    // Format data with timestamp
    // Write to SD card with appropriate file structure
}

```

### Data Collection Process
1. System initializes SD card and sensors in `setup()`
3. Current and voltage are measured using the ADC
4. Readings are converted to appropriate units using calibration factors
5. Data is written to timestamp-formatted files on the SD card
6. Files are organized by date for easy analysis

## Data File Structure

The system generates data files in the following locations:
```
visualization/files/YYYY-MM-DD/Amps YYYY-MM-DD.txt
visualization/files/YYYY-MM-DD/Volts YYYY-MM-DD.txt
```

Each data file contains timestamped readings in the format:
```
HH:MM:SS --> value1, value2, ...
```

## Usage

### Data Collection
1. Connect the ESP32 system to your battery
2. Power on the system
3. Data will be automatically logged to the SD card
4. Transfer the SD card data to your computer in the appropriate folder structure

### Data Visualization
To visualize battery data, run:

```bash
python visualization/Volts_vs_Amps.py [YYYY-MM-DD]
```

- If a date is provided, data for that specific date will be plotted
- If no date is provided, the latest available data will be used
- The script automatically finds the appropriate data files and creates visualization plots

## Output

The script generates:
1. A combined visualization showing all metrics on a single time-axis plot
2. Summary statistics in the console output
3. Saved plot images in the `visualization/plots` directory

