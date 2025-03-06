import sys
import os
import re
import datetime
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import numpy as np


def read_data_file(file_path):
    """
    Reads data from a file where each line represents one minute of data.
    
    The file format is:
    - First row may contain values without timestamp
    - Subsequent rows have a timestamp (HH:MM:SS) followed by '-->'
    - After the '-->' are comma-separated values, one for each second
    
    Returns two lists:
    - timestamps: List of datetime objects with second-level precision
    - values: List of corresponding measurement values
    """
    with open(file_path, 'r') as file:
        lines = file.readlines()
    
    timestamps = []
    values = []
    current_minute = None
    
    for line in lines:
        line = line.strip()
        if not line:
            continue  # Skip empty lines
            
        # Check if line has a timestamp marker
        if '-->' in line:
            parts = line.split('-->')
            timestamp_str = parts[0].strip()
            data_part = parts[1].strip()
            
            # Parse the timestamp (usually in HH:MM:SS format)
            try:
                # This gives us the minute marker
                time_obj = datetime.datetime.strptime(timestamp_str, "%H:%M:%S")
                current_minute = time_obj
            except ValueError:
                try:
                    # Try without seconds
                    time_obj = datetime.datetime.strptime(timestamp_str, "%H:%M")
                    current_minute = time_obj
                except ValueError:
                    print(f"Warning: Could not parse timestamp: {timestamp_str}")
                    continue
        else:
            # Line without timestamp - use data directly
            data_part = line
        
        # Parse the comma-separated values
        second_values = []
        for val in data_part.split(','):
            val = val.strip()
            if val:  # Skip empty values
                try:
                    second_values.append(float(val))
                except ValueError:
                    print(f"Warning: Could not parse value: {val}")
        
        # If we have a current minute and values, create timestamps for each second
        if current_minute and second_values:
            for second_idx, value in enumerate(second_values):
                # Create a timestamp with the correct second
                second_timestamp = current_minute.replace(second=second_idx % 60)
                
                # If we're about to overflow to the next minute (would have second >= 60),
                # then increment the minute appropriately
                if second_idx >= 60:
                    extra_minutes = second_idx // 60
                    second_timestamp += datetime.timedelta(minutes=extra_minutes)
                
                timestamps.append(second_timestamp)
                values.append(value)
    
    if not timestamps:
        raise ValueError(f"No valid data found in file: {file_path}")
        
    return timestamps, values


def find_date_directories(base_dir="visualization/data"):
    """
    Finds all date directories in the format YYYY-MM-DD.
    Returns a list of (date, directory_path) tuples sorted by date.
    """
    date_dirs = []
    
    if not os.path.exists(base_dir):
        print(f"Base directory '{base_dir}' does not exist.")
        return date_dirs
    
    for item in os.listdir(base_dir):
        # First check if it's a directory
        dir_path = os.path.join(base_dir, item)
        if os.path.isdir(dir_path):
            # Try to parse the directory name as a date
            try:
                date_obj = datetime.datetime.strptime(item, "%Y-%m-%d").date()
                date_dirs.append((date_obj, dir_path))
            except ValueError:
                # Not a date-formatted directory, skip it
                continue
    
    # Sort directories by date
    return sorted(date_dirs, key=lambda x: x[0])


def load_data_from_directory(dir_path, reference_date):
    """
    Loads voltage and current data from "Amps YYYY-MM-DD.txt" and "Volts YYYY-MM-DD.txt" in the specified directory.
    Applies the reference date to timestamps for plotting.
    """
    date_str = reference_date.strftime("%Y-%m-%d")
    
    # Check for Amps file in the directory
    amps_file = os.path.join(dir_path, f"Amps {date_str}.txt")
    amps_found = os.path.exists(amps_file)
    
    if not amps_found:
        fallback_amps = os.path.join(dir_path, "Amps.txt")
        if os.path.exists(fallback_amps):
            print(f"Using fallback file: {fallback_amps}")
            amps_file = fallback_amps
            amps_found = True
        else:
            # Try to find in parent directory
            parent_dir = os.path.dirname(dir_path)
            parent_amps = os.path.join(parent_dir, f"Amps {date_str}.txt")
            if os.path.exists(parent_amps):
                print(f"Using file from parent directory: {parent_amps}")
                amps_file = parent_amps
                amps_found = True
    
    # Check for Volts file in the directory
    volts_file = os.path.join(dir_path, f"Volts {date_str}.txt")
    volts_found = os.path.exists(volts_file)
    
    if not volts_found:
        fallback_volts = os.path.join(dir_path, "Volts.txt")
        if os.path.exists(fallback_volts):
            print(f"Using fallback file: {fallback_volts}")
            volts_file = fallback_volts
            volts_found = True
        else:
            # Try to find in parent directory
            parent_dir = os.path.dirname(dir_path)
            parent_volts = os.path.join(parent_dir, f"Volts {date_str}.txt")
            if os.path.exists(parent_volts):
                print(f"Using file from parent directory: {parent_volts}")
                volts_file = parent_volts
                volts_found = True
    
    # Check if at least one file type was found
    if not amps_found and not volts_found:
        raise FileNotFoundError(f"No data files found for date {date_str} in directory {dir_path} or parent directory")
    
    # Handle case where only one type of file is found
    timestamps = None
    voltages = []
    currents = []
    
    # Load amps data if available
    if amps_found:
        print(f"Loading current data from: {amps_file}")
        amps_timestamps, amps_values = read_data_file(amps_file)
        if timestamps is None:
            timestamps = amps_timestamps
        currents = amps_values
    else:
        print(f"Warning: No current data file found for date {date_str}")
        amps_timestamps = []
        amps_values = []
    
    # Load volts data if available
    if volts_found:
        print(f"Loading voltage data from: {volts_file}")
        volts_timestamps, volts_values = read_data_file(volts_file)
        if timestamps is None:
            timestamps = volts_timestamps
        voltages = volts_values
    else:
        print(f"Warning: No voltage data file found for date {date_str}")
        volts_timestamps = []
        volts_values = []
    
    # If both files are present, align the data
    if amps_found and volts_found:
        timestamps, voltages, currents = align_data(amps_timestamps, amps_values, 
                                                  volts_timestamps, volts_values)
    elif amps_found:
        # Only amps data available
        timestamps = amps_timestamps
        currents = amps_values
        voltages = [0] * len(timestamps)  # Placeholder zeros for missing voltage data
    elif volts_found:
        # Only volts data available
        timestamps = volts_timestamps
        voltages = volts_values
        currents = [0] * len(timestamps)  # Placeholder zeros for missing current data
    
    # Apply reference date to timestamps
    dated_timestamps = []
    for ts in timestamps:
        full_dt = datetime.datetime.combine(reference_date, 
                                          datetime.time(ts.hour, ts.minute, ts.second))
        dated_timestamps.append(full_dt)
    
    return dated_timestamps, voltages, currents


def align_data(amps_timestamps, amps_values, volts_timestamps, volts_values):
    """
    Aligns current and voltage data based on timestamps.
    Returns common timestamps and corresponding values.
    Handles cases where one of the data arrays might be empty.
    """
    # Handle special cases where one dataset might be empty
    if not amps_timestamps or not amps_values:
        print("Warning: No current data available for alignment")
        return volts_timestamps, volts_values, [0] * len(volts_values)
    
    if not volts_timestamps or not volts_values:
        print("Warning: No voltage data available for alignment")
        return amps_timestamps, [0] * len(amps_values), amps_values
    
    # Create mappings of timestamp to value for easy lookup
    amps_map = {}
    for i, ts in enumerate(amps_timestamps):
        if i < len(amps_values):
            amps_map[ts.strftime("%H:%M:%S")] = amps_values[i]
    
    volts_map = {}
    for i, ts in enumerate(volts_timestamps):
        if i < len(volts_values):
            volts_map[ts.strftime("%H:%M:%S")] = volts_values[i]
    
    # Find common timestamps
    common_timestamps = []
    common_volts = []
    common_amps = []
    
    # Use all timestamps from amps file as reference and find matching voltage values
    for i, ts in enumerate(amps_timestamps):
        time_key = ts.strftime("%H:%M:%S")
        if time_key in volts_map and time_key in amps_map:
            common_timestamps.append(ts)
            common_volts.append(volts_map[time_key])
            common_amps.append(amps_map[time_key])
    
    if not common_timestamps:
        print("Warning: No exact timestamp matches between current and voltage files.")
        print("Using current timestamps and aligning voltage data.")
        
        # Use all amps timestamps and closest voltage values
        min_length = min(len(amps_timestamps), len(volts_values))
        return amps_timestamps[:min_length], volts_values[:min_length], amps_values[:min_length]
    
    return common_timestamps, common_volts, common_amps


def compute_cumulative_capacity(timestamps, current_values):
    """
    Computes cumulative capacity in Ah, only counting discharge current (positive values).
    """
    cumulative_capacity = [0]
    
    for i in range(1, len(current_values)):
        # Calculate time difference in hours
        dt = (timestamps[i] - timestamps[i-1]).total_seconds() / 3600
        current = current_values[i-1]  # Using previous current value
        
        # Only discharge (positive current) reduces the capacity
        if current > 0:
            delta_capacity = current * dt
            cumulative_capacity.append(cumulative_capacity[-1] + delta_capacity)
        else:
            cumulative_capacity.append(cumulative_capacity[-1])
    
    return np.array(cumulative_capacity)


def plot_combined_data(timestamps, voltages, currents, capacity_values, plot_date):
    """
    Creates a single visualization with all three metrics overlapping:
    - Current (A) over time
    - Voltage (V) over time 
    - Cumulative capacity (Ah) over time
    All metrics share the same x-axis but have their own y-axes.
    """
    # Create figure and primary axis
    fig, ax1 = plt.subplots(figsize=(15, 8))
    fig.suptitle(f"Battery Data Analysis - {plot_date.strftime('%Y-%m-%d')}", fontsize=16)
    
    # Get time range for proper formatting
    if timestamps:
        min_time = min(timestamps)
        max_time = max(timestamps)
        total_seconds = (max_time - min_time).total_seconds()
        hours_span = total_seconds / 3600
    else:
        hours_span = 1  # Default if no data
    
    # Choose appropriate time formatting based on data span
    if hours_span < 0.1:  # Less than 6 minutes of data
        # For very short spans, show minute:second with second precision
        major_formatter = mdates.DateFormatter('%H:%M:%S')
        major_locator = mdates.SecondLocator(bysecond=range(0, 60, 10))  # Every 10 seconds
        minor_locator = mdates.SecondLocator(bysecond=range(0, 60, 1))   # Every second
    elif hours_span < 1:
        # For less than an hour of data, show minute:second
        major_formatter = mdates.DateFormatter('%H:%M:%S')
        major_locator = mdates.MinuteLocator(byminute=range(0, 60, 1))   # Every minute
        minor_locator = mdates.SecondLocator(bysecond=range(0, 60, 15))  # Every 15 seconds
    elif hours_span < 6:
        # For 1-6 hours, show hour:minute with 15-minute ticks
        major_formatter = mdates.DateFormatter('%H:%M')
        major_locator = mdates.MinuteLocator(byminute=range(0, 60, 15))  # Every 15 minutes
        minor_locator = mdates.MinuteLocator(byminute=range(0, 60, 5))   # Every 5 minutes
    else:
        # For longer periods, show hour with hourly ticks
        major_formatter = mdates.DateFormatter('%H:%M')
        major_locator = mdates.HourLocator()
        minor_locator = mdates.MinuteLocator(byminute=[0, 30])  # Every 30 minutes
    
    # Apply the formatting to the x-axis
    ax1.xaxis.set_major_formatter(major_formatter)
    ax1.xaxis.set_major_locator(major_locator)
    ax1.xaxis.set_minor_locator(minor_locator)
    
    # Make sure we show grid lines for easier reading
    ax1.grid(True, which='major', linestyle='-', alpha=0.7)
    ax1.grid(True, which='minor', linestyle=':', alpha=0.4)
    
    # Ensure all data points are plotted by setting the limit to actual data range
    if timestamps:
        ax1.set_xlim(min_time, max_time)
        
        # Add interactive time information with tooltips for precise second values
        def format_coord(x, y):
            # Convert x (float date) to datetime
            date_obj = mdates.num2date(x)
            # Format with full precision including seconds
            time_str = date_obj.strftime('%H:%M:%S')
            # Return formatted coordinate string with both time and y-value
            return f'Time: {time_str}, Value: {y:.2f}'
            
        ax1.format_coord = format_coord
    
    # Check if we have current data (not all zeros)
    has_current_data = any(abs(current) > 0.001 for current in currents)
    
    # Plot 1: Current over time (primary y-axis) if data exists
    color_current = 'black'
    ax1.set_xlabel(f"Time on {plot_date.strftime('%Y-%m-%d')}")
    ax1.set_ylabel("Current [A]", color=color_current)
    
    if has_current_data:
        # Plot every data point with markers
        ax1.plot(timestamps, currents, color=color_current, linewidth=2.0, marker='.', 
               markersize=3, label='Current [A]')
        
        # Fill between to highlight charging/discharging areas
        ax1.fill_between(timestamps, currents, 0, where=(np.array(currents) >= 0), 
                      interpolate=True, color='blue', alpha=0.15, label='Charging')
        ax1.fill_between(timestamps, currents, 0, where=(np.array(currents) < 0), 
                      interpolate=True, color='red', alpha=0.15, label='Discharging')
    else:
        print("Warning: No current data to plot")
    
    # Check if we have voltage data (not all zeros)
    has_voltage_data = any(abs(voltage) > 0.001 for voltage in voltages)
    
    # Create second y-axis and plot voltage
    ax2 = ax1.twinx()
    color_voltage = 'green'
    ax2.set_ylabel("Voltage [V]", color=color_voltage)
    
    if has_voltage_data:
        # Plot with markers to show individual data points
        ax2.plot(timestamps, voltages, color=color_voltage, linewidth=2.0, 
               marker='.', markersize=3, label='Voltage [V]')
    else:
        print("Warning: No voltage data to plot")
    
    # Create third y-axis and plot capacity
    # Only proceed if we have current data to calculate capacity
    if has_current_data:
        # Offset third y-axis to the right
        ax3 = ax1.twinx()
        ax3.spines["right"].set_position(("axes", 1.15))
        
        color_capacity = 'red'
        ax3.set_ylabel("Capacity [Ah]", color=color_capacity)
        
        # Plot with markers to show individual data points
        ax3.plot(timestamps, capacity_values, color=color_capacity, linewidth=2.0, 
               marker='.', markersize=3, label='Capacity [Ah]')
        
        # Set the y-limit for capacity to start from 0
        max_capacity = max(capacity_values) if max(capacity_values) > 0 else 1
        ax3.set_ylim(0, max_capacity * 1.1)
    
    # Create combined legend
    lines1, labels1 = ax1.get_legend_handles_labels()
    
    if has_voltage_data:
        lines2, labels2 = ax2.get_legend_handles_labels()
    else:
        lines2, labels2 = [], []
    
    if has_current_data:
        lines3, labels3 = ax3.get_legend_handles_labels() if 'ax3' in locals() else ([], [])
    else:
        lines3, labels3 = [], []
    
    # Combine only non-empty legend elements
    all_lines = []
    all_labels = []
    
    if lines1:
        all_lines.extend(lines1)
        all_labels.extend(labels1)
    
    if lines2:
        all_lines.extend(lines2)
        all_labels.extend(labels2)
    
    if lines3:
        all_lines.extend(lines3)
        all_labels.extend(labels3)
    
    # Only create a legend if we have items for it
    if all_lines:
        ax1.legend(all_lines, all_labels, loc='upper center', bbox_to_anchor=(0.5, -0.15), 
                ncol=3, fancybox=True, shadow=True)
    
    # Add grid
    ax1.grid(True, linestyle='--', alpha=0.7)
    
    # Rotate x-axis labels for better readability and adjust spacing
    plt.setp(ax1.get_xticklabels(), rotation=45, ha='right')
    
    # Add time range annotation at the bottom
    if timestamps:
        start_time = min(timestamps).strftime('%H:%M:%S')
        end_time = max(timestamps).strftime('%H:%M:%S')
        time_range_text = f"Time Range: {start_time} to {end_time}"
        fig.text(0.5, 0.01, time_range_text, ha='center', fontsize=10)
    
    # Make sure the minor ticks are also visible with proper formatting
    ax1.tick_params(which='minor', length=4, color='k', width=1.0)
    ax1.tick_params(which='major', length=7, color='k', width=1.5)
    
    # Add timestamp markers and make sure tick labels don't overlap
    fig.autofmt_xdate()
    
    # Improve layout with extra space for x-axis labels
    plt.tight_layout()
    plt.subplots_adjust(right=0.85, bottom=0.2)
    
    # Show the figure first
    plt.show()
    
    # Ask user if they want to save the figure
    save_response = input(f"Save plot to file? (y/n): ").strip().lower()
    
    if save_response == 'y' or save_response == 'yes':
        save_dir = "visualization/plots"
        if not os.path.exists(save_dir):
            os.makedirs(save_dir)
        
        filename = os.path.join(save_dir, f"battery_data_{plot_date.strftime('%Y-%m-%d')}.png")
        fig.savefig(filename, dpi=300)
        print(f"Plot saved to: {filename}")
    else:
        print("Plot not saved.")
    
    plt.close(fig)


def main():
    base_dir = "visualization/data"
    
    # Check if files directory exists
    if not os.path.exists(base_dir):
        print(f"Error: Directory '{base_dir}' does not exist.")
        return
    
    # Find date directories
    date_dirs = find_date_directories(base_dir)
    
    if not date_dirs:
        print(f"No valid date directories found in '{base_dir}'.")
        print("Please create date folders (YYYY-MM-DD) containing Amps.txt and Volts.txt files.")
        return
    
    # Display available dates
    print("Available dates:")
    for date, _ in date_dirs:
        print(f"  - {date}")
    
    # Check if a date argument was provided
    if len(sys.argv) > 1:
        try:
            requested_date = datetime.datetime.strptime(sys.argv[1], "%Y-%m-%d").date()
            
            # Look for matching directory
            matching_dirs = [d for d in date_dirs if d[0] == requested_date]
            
            if matching_dirs:
                selected_date, selected_dir = matching_dirs[0]
            else:
                print(f"No data found for date {requested_date}.")
                print("Using the latest date instead.")
                selected_date, selected_dir = date_dirs[-1]  # Use the latest
        except ValueError:
            print("Invalid date format. Please use YYYY-MM-DD.")
            print("Using the latest date instead.")
            selected_date, selected_dir = date_dirs[-1]  # Use the latest
    else:
        # No argument provided; use the latest date
        selected_date, selected_dir = date_dirs[-1]  # Use the latest
    
    print(f"Using data for date: {selected_date}")
    
    try:
        # Load data from the selected directory
        timestamps, voltages, currents = load_data_from_directory(selected_dir, selected_date)
        
        # Print debug info to verify second-level resolution
        print(f"Loaded {len(timestamps)} data points")
        print(f"First 5 timestamps:")
        for i in range(min(5, len(timestamps))):
            print(f"  {timestamps[i].strftime('%H:%M:%S')}: {currents[i]}")
            
        print(f"Last 5 timestamps:")
        for i in range(max(0, len(timestamps)-5), len(timestamps)):
            print(f"  {timestamps[i].strftime('%H:%M:%S')}: {currents[i]}")
        
        # Calculate cumulative capacity
        capacity_values = compute_cumulative_capacity(timestamps, currents)
        
        # Create combined plot with all three metrics
        plot_combined_data(timestamps, voltages, currents, capacity_values, selected_date)
        
        # Print summary information
        print("Summary:")
        print(f"Max Current: {max(currents):.2f} A")
        print(f"Min Current: {min(currents):.2f} A")
        print(f"Max Voltage: {max(voltages):.2f} V")
        print(f"Min Voltage: {min(voltages):.2f} V")
        print(f"Final Cumulative Capacity: {capacity_values[-1]:.2f} Ah")
    
    except Exception as e:
        print(f"Error processing data: {e}")
        import traceback
        traceback.print_exc()


if __name__ == '__main__':
    main()
