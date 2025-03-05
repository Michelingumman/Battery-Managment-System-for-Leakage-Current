import sys
import os
import re
import datetime
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import numpy as np


def read_data_file(file_path):
    """
    Reads data from a file. Each line should contain a timestamp and comma-separated values,
    separated by '-->'.
    """
    with open(file_path, 'r') as file:
        lines = file.readlines()
    
    timestamps = []
    values = []
    
    for line in lines:
        if '-->' in line:
            timestamp_part = line.split('-->')[0].strip()
            numbers_part = line.split('-->')[-1]
            
            # Parse timestamp (HH:MM:SS)
            try:
                time_obj = datetime.datetime.strptime(timestamp_part, "%H:%M:%S")
            except ValueError:
                # Try without seconds if that fails
                time_obj = datetime.datetime.strptime(timestamp_part, "%H:%M")
            
            timestamps.append(time_obj)
            
            # Parse values - split by comma and convert to float
            data_values = [float(x.strip()) for x in numbers_part.strip().split(',') if x.strip()]
            
            # If there are multiple values per timestamp, average them
            if data_values:
                avg_value = sum(data_values) / len(data_values)
                values.append(avg_value)
    
    if not timestamps:
        raise ValueError(f"No valid data found in file: {file_path}")
        
    return timestamps, values


def find_date_directories(base_dir="visualization/files"):
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
    Loads voltage and current data from Amps.txt and Volts.txt in the specified directory.
    Applies the reference date to timestamps for plotting.
    """
    amps_file = os.path.join(dir_path, "Amps.txt")
    volts_file = os.path.join(dir_path, "Volts.txt")
    
    if not os.path.exists(amps_file):
        raise FileNotFoundError(f"Current data file not found: {amps_file}")
    
    if not os.path.exists(volts_file):
        raise FileNotFoundError(f"Voltage data file not found: {volts_file}")
    
    # Read current data
    amps_timestamps, amps_values = read_data_file(amps_file)
    
    # Read voltage data
    volts_timestamps, volts_values = read_data_file(volts_file)
    
    # Align the data based on timestamps
    timestamps, voltages, currents = align_data(amps_timestamps, amps_values, 
                                               volts_timestamps, volts_values)
    
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
    Returns synchronized timestamps, voltages, and currents.
    """
    # Create a mapping of timestamps to values
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
    fig, ax1 = plt.subplots(figsize=(12, 8))
    fig.suptitle(f"Battery Data Analysis - {plot_date.strftime('%Y-%m-%d')}", fontsize=16)
    
    # Format x-axis to show date and time
    formatter = mdates.DateFormatter('%H:%M')
    ax1.xaxis.set_major_formatter(formatter)
    ax1.xaxis.set_major_locator(mdates.HourLocator())
    
    # Plot 1: Current over time (primary y-axis)
    color_current = 'black'
    ax1.set_xlabel(f"Time on {plot_date.strftime('%Y-%m-%d')}")
    ax1.set_ylabel("Current [A]", color=color_current)
    ax1.plot(timestamps, currents, color=color_current, linewidth=2.5, label='Current [A]')
    
    # Fill between to highlight charging/discharging areas
    ax1.fill_between(timestamps, currents, 0, where=(np.array(currents) >= 0), 
                  interpolate=True, color='blue', alpha=0.15, label='Charging')
    ax1.fill_between(timestamps, currents, 0, where=(np.array(currents) < 0), 
                  interpolate=True, color='red', alpha=0.15, label='Discharging')
    
    # Add a horizontal line at 0 for reference
    ax1.axhline(0, color='gray', linestyle='--', linewidth=1)
    ax1.tick_params(axis='y', labelcolor=color_current)
    
    # Create second y-axis for voltage
    ax2 = ax1.twinx()
    color_voltage = 'darkgreen'
    ax2.set_ylabel("Voltage [V]", color=color_voltage)
    ax2.plot(timestamps, voltages, color=color_voltage, linewidth=2.5, label='Voltage [V]')
    ax2.tick_params(axis='y', labelcolor=color_voltage)
    
    # Create third y-axis for capacity (by offsetting the second axis)
    ax3 = ax1.twinx()
    # Move the third axis to the right
    ax3.spines["right"].set_position(("axes", 1.15))
    color_capacity = 'purple'
    ax3.set_ylabel("Capacity [Ah]", color=color_capacity)
    ax3.plot(timestamps, capacity_values, color=color_capacity, linestyle='--', linewidth=2.5, label='Capacity [Ah]')
    ax3.tick_params(axis='y', labelcolor=color_capacity)
    
    # Add grid (but only for the main axis)
    ax1.grid(True, alpha=0.3)
    
    # Create a combined legend
    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    lines3, labels3 = ax3.get_legend_handles_labels()
    
    # Filter out the fill_between labels to avoid duplication
    filtered_lines1 = [lines1[0]]  # Only keep the first line (Current)
    filtered_labels1 = [labels1[0]]
    
    ax1.legend(filtered_lines1 + lines2 + lines3, filtered_labels1 + labels2 + labels3, 
              loc='upper center', bbox_to_anchor=(0.5, -0.10), ncol=3, frameon=True)
    
    # Add time range annotation
    time_range_text = f"Time Range: {timestamps[0].strftime('%H:%M:%S')} to {timestamps[-1].strftime('%H:%M:%S')}"
    plt.figtext(0.5, 0.01, time_range_text, ha='center', fontsize=10)
    
    # Add charging/discharging legend separately
    charge_patch = plt.Rectangle((0, 0), 1, 1, color='red', alpha=0.15)
    discharge_patch = plt.Rectangle((0, 0), 1, 1, color='blue', alpha=0.15)
    plt.legend([charge_patch, discharge_patch], ['Discharging', 'Charging'], 
              loc='upper right', framealpha=0.7)
    
    # Rotate x-axis labels for better readability
    plt.setp(ax1.get_xticklabels(), rotation=45, ha='right')
    
    # Improve layout
    plt.tight_layout()
    plt.subplots_adjust(right=0.85, bottom=0.15)
    
    # Save the figure
    save_dir = "plots"
    if not os.path.exists(save_dir):
        os.makedirs(save_dir)
    
    fig.savefig(os.path.join(save_dir, f"battery_data_{plot_date.strftime('%Y-%m-%d')}.png"), dpi=300)
    
    # Show the figure
    plt.show()
    plt.close(fig)


def main():
    base_dir = "visualization/files"
    
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
