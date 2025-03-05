# Battery Management System for Leakage Current

A comprehensive system for monitoring, analyzing, and visualizing battery charging and discharging patterns, with a focus on leakage current measurement.

## Overview

This project provides tools to collect, process, and visualize battery data, helping to understand battery performance, discharge patterns, and capacity utilization. The system works with time-series data of voltage and current measurements to create detailed performance visualizations.

## Features

- **Data Collection**: Records battery voltage and current measurements over time
- **Advanced Visualization**: Creates multi-axis plots showing:
  - Current (A) over time
  - Voltage (V) over time
  - Cumulative discharge capacity (Ah)
  - Battery percentage remaining (based on 55Ah total capacity)
- **Battery Analytics**: Calculates important metrics like:
  - Cumulative capacity usage
  - Battery percentage remaining
  - Charging and discharging patterns
- **Flexible Data Handling**: Works with various file formats and directory structures

## Data File Structure

The system expects data files in the following locations:
```
visualization/files/YYYY-MM-DD/Amps YYYY-MM-DD.txt
visualization/files/YYYY-MM-DD/Volts YYYY-MM-DD.txt
```

Each data file should contain timestamped readings in the format:
```
HH:MM:SS --> value1, value2, ...
```

## Usage

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
3. Saved plot images in the `plots` directory

## Dependencies

- Python 3.x
- NumPy
- Matplotlib

## Example Output

The visualization displays:
- Black line: Current measurements (A)
- Green line: Voltage measurements (V)
- Red line: Cumulative discharge (Ah)  
- Purple dashed line: Battery percentage remaining (%)

Blue/red shading indicates charging/discharging periods.

Summary statistics show maximum and minimum values for current and voltage, final cumulative discharge, and remaining battery capacity as a percentage of the 55Ah total capacity.
