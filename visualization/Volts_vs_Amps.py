import sys
import os
import re
import datetime
import matplotlib.pyplot as plt
import numpy as np
#Test

def read_file(file_name):
    """
    Reads data from a file. Each line should contain a timestamp and comma-separated values (voltage, current),
    separated by '-->'. If an error occurs, sample data is generated.
    """
    try:
        with open(file_name, 'r') as file:
            lines = file.readlines()
        
        timestamps = []
        voltages = []
        currents = []
        
        for line in lines:
            if '-->' in line:
                timestamp_part = line.split('-->')[0].strip()
                numbers_part = line.split('-->')[-1]
                
                timestamps.append(datetime.datetime.strptime(timestamp_part, "%H:%M:%S"))
                values = [float(x) for x in numbers_part.strip().split(',') if x]
                
                # Assuming the format is: voltage, current
                if len(values) >= 2:
                    voltages.append(values[0])
                    currents.append(values[1])
        
        return timestamps, voltages, currents
    except Exception as e:
        print(f"Error reading file: {e}. Generating sample data.")
        base_time = datetime.datetime.strptime("08:00:00", "%H:%M:%S")
        timestamps = [base_time + datetime.timedelta(seconds=60 * i) for i in range(120)]
        
        # Generate sample voltage data (around 12V with small fluctuations)
        voltages = [12 + 0.5 * np.sin(i/10) for i in range(120)]
        
        # Generate sample current data
        t = np.linspace(0, 4 * np.pi, 120)
        currents = np.sin(t).tolist()
        
        return timestamps, voltages, currents


def compute_cumulative_capacity(time_seconds, current_values):
    """
    Computes cumulative capacity in Ah, only counting discharge current (positive values).
    """
    cumulative_capacity = [0]
    
    for i in range(1, len(current_values)):
        dt = (time_seconds[i] - time_seconds[i-1]) / 3600  # Convert seconds to hours
        current = current_values[i-1]  # Using previous current value
        
        # Only discharge (positive current) reduces the capacity
        if current > 0:
            delta_capacity = current * dt
            cumulative_capacity.append(cumulative_capacity[-1] + delta_capacity)
        else:
            cumulative_capacity.append(cumulative_capacity[-1])
    
    return np.array(cumulative_capacity)


def plot_current_over_time(x_minutes, currents, start_time, end_time):
    """Plot current over time."""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    # Plot the current data line
    ax.plot(x_minutes, currents, color='black', linewidth=2, label='Current [A]')
    
    # Fill between to highlight charging (blue for current >= 0) and discharging (red for current < 0)
    ax.fill_between(x_minutes, currents, 0, where=(np.array(currents) >= 0), 
                    interpolate=True, color='blue', alpha=0.3, label='Charging')
    ax.fill_between(x_minutes, currents, 0, where=(np.array(currents) < 0), 
                    interpolate=True, color='red', alpha=0.3, label='Discharging')
    
    # Add a horizontal line at 0 for reference
    ax.axhline(0, color='gray', linestyle='--', linewidth=1)
    
    ax.set_title("Current Over Time")
    ax.set_xlabel("Time [minutes]")
    ax.set_ylabel("Current [A]")
    
    # Annotate with start and end times
    ax.text(0.05, 0.95, f"Start: {start_time.strftime('%H:%M:%S')}\nEnd: {end_time.strftime('%H:%M:%S')}",
            transform=ax.transAxes, fontsize=10, verticalalignment='top',
            bbox=dict(facecolor='white', alpha=0.7))
    
    ax.grid(True)
    ax.legend(loc='best')
    
    plt.tight_layout()
    plt.show()
    plt.close(fig)


def plot_voltage_over_time(x_minutes, voltages, start_time, end_time):
    """Plot voltage over time."""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    # Plot the voltage data line
    ax.plot(x_minutes, voltages, color='darkgreen', linewidth=2, label='Voltage [V]')
    
    ax.set_title("Voltage Over Time")
    ax.set_xlabel("Time [minutes]")
    ax.set_ylabel("Voltage [V]")
    
    # Annotate with start and end times
    ax.text(0.05, 0.95, f"Start: {start_time.strftime('%H:%M:%S')}\nEnd: {end_time.strftime('%H:%M:%S')}",
            transform=ax.transAxes, fontsize=10, verticalalignment='top',
            bbox=dict(facecolor='white', alpha=0.7))
    
    ax.grid(True)
    ax.legend(loc='best')
    
    plt.tight_layout()
    plt.show()
    plt.close(fig)


def plot_cumulative_capacity(x_minutes, capacity_values, start_time, end_time):
    """Plot cumulative capacity over time."""
    fig, ax = plt.subplots(figsize=(10, 6))
    
    # Plot the capacity data line
    ax.plot(x_minutes, capacity_values, color='purple', linewidth=2, label='Cumulative Capacity [Ah]')
    
    ax.set_title("Cumulative Capacity Over Time")
    ax.set_xlabel("Time [minutes]")
    ax.set_ylabel("Capacity [Ah]")
    
    # Annotate with start and end times
    ax.text(0.05, 0.95, f"Start: {start_time.strftime('%H:%M:%S')}\nEnd: {end_time.strftime('%H:%M:%S')}",
            transform=ax.transAxes, fontsize=10, verticalalignment='top',
            bbox=dict(facecolor='white', alpha=0.7))
    
    ax.grid(True)
    ax.legend(loc='best')
    
    plt.tight_layout()
    plt.show()
    plt.close(fig)


def find_latest_file(directory="files"):
    """
    Finds the file with the latest date in the given directory.
    Files must have a name in the format YYYY-MM-DD.txt.
    Returns None if no valid files found or directory doesn't exist.
    """
    try:
        if not os.path.exists(directory):
            os.makedirs(directory)
            print(f"Created directory: {directory}")
            return None
            
        date_files = []
        for f in os.listdir(directory):
            match = re.match(r'(\d{4}-\d{2}-\d{2})\.txt$', f)
            if match:
                date_str = match.group(1)
                try:
                    file_date = datetime.datetime.strptime(date_str, "%Y-%m-%d").date()
                    date_files.append((file_date, f))
                except ValueError:
                    continue
        if not date_files:
            return None
        latest_file = max(date_files, key=lambda x: x[0])
        return os.path.join(directory, latest_file[1]), latest_file[0]
    except Exception as e:
        print(f"Error finding latest file: {e}")
        return None


def generate_sample_data(plot_date):
    """
    Generates sample data for the given date.
    Returns timestamps, voltages, and currents.
    """
    print(f"Generating sample data for: {plot_date}")
    
    # Create timestamps for a full day, one sample every 15 minutes
    base_time = datetime.datetime.combine(plot_date, datetime.time(0, 0))
    timestamps = [base_time + datetime.timedelta(minutes=15*i) for i in range(96)]  # 24h * 4 samples/hour
    
    # Generate sample voltage data (around 12V with day/night cycle)
    hour_angles = np.array([(t.hour + t.minute/60) * 15 for t in timestamps])  # Convert hours to degrees
    voltages = [12.5 + 0.5 * np.sin(np.radians(h)) for h in hour_angles]
    
    # Generate sample current data with charging during day, discharging at night
    currents = []
    for t in timestamps:
        hour = t.hour + t.minute/60
        if 6 <= hour < 18:  # Daytime (6 AM to 6 PM)
            # Charging (negative current in range -2 to -0.5)
            currents.append(-1.5 + 0.5 * np.sin(np.radians(hour * 30)))
        else:  # Nighttime
            # Discharging (positive current in range 0.3 to 1.2)
            currents.append(0.75 + 0.45 * np.sin(np.radians(hour * 30)))
    
    return timestamps, voltages, currents


def main():
    directory = "files"
    
    # Ensure directory exists
    if not os.path.exists(directory):
        os.makedirs(directory)
        print(f"Created directory: {directory}")
    
    # Check if a date argument was provided
    if len(sys.argv) > 1:
        try:
            plot_date = datetime.datetime.strptime(sys.argv[1], "%Y-%m-%d").date()
            file_name = os.path.join(directory, f"{plot_date.strftime('%Y-%m-%d')}.txt")
            
            if os.path.exists(file_name):
                print(f"Using data file: {file_name}")
                timestamps, voltages, currents = read_file(file_name)
            else:
                print(f"File for date {plot_date} does not exist. Using sample data instead.")
                timestamps, voltages, currents = generate_sample_data(plot_date)
        except ValueError:
            print("Invalid date format. Please use YYYY-MM-DD. Using latest file or sample data.")
            result = find_latest_file(directory)
            if result is None:
                # No valid files found, use current date
                plot_date = datetime.datetime.now().date()
                timestamps, voltages, currents = generate_sample_data(plot_date)
            else:
                file_name, plot_date = result
                print(f"Using latest data file: {file_name}")
                timestamps, voltages, currents = read_file(file_name)
    else:
        # No argument provided; find the latest file
        result = find_latest_file(directory)
        if result is None:
            # No valid files found, use current date
            plot_date = datetime.datetime.now().date()
            timestamps, voltages, currents = generate_sample_data(plot_date)
        else:
            file_name, plot_date = result
            print(f"Using latest data file: {file_name}")
            timestamps, voltages, currents = read_file(file_name)

    print(f"Plotting data for: {plot_date}")
    
    # Create time in seconds for calculations
    x_seconds = np.array([(t - timestamps[0]).total_seconds() for t in timestamps])
    
    # Convert to minutes for plotting
    x_minutes = x_seconds / 60.0
    
    # Adjust for winter time by subtracting one hour from the timestamps
    start_time = timestamps[0] - datetime.timedelta(hours=1)
    end_time = timestamps[-1] - datetime.timedelta(hours=1)
    
    # Calculate cumulative capacity
    cumulative_capacity = compute_cumulative_capacity(x_seconds, currents)
    
    # Create all three plots
    plot_current_over_time(x_minutes, currents, start_time, end_time)
    plot_voltage_over_time(x_minutes, voltages, start_time, end_time)
    plot_cumulative_capacity(x_minutes, cumulative_capacity, start_time, end_time)
    
    # Print summary information
    print("Summary:")
    print(f"Max Current: {max(currents):.2f} A")
    print(f"Min Current: {min(currents):.2f} A")
    print(f"Max Voltage: {max(voltages):.2f} V")
    print(f"Min Voltage: {min(voltages):.2f} V")
    print(f"Final Cumulative Capacity: {cumulative_capacity[-1]:.2f} Ah")


if __name__ == '__main__':
    main()
