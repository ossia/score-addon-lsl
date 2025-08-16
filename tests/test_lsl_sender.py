#!/usr/bin/env python3
"""
LSL Test Sender - Sends fake sensor data over LSL
This script creates an LSL outlet and sends simulated sensor data.
"""

import pylsl
import time
import math
import random

def main():
    # Create LSL stream info
    # We'll simulate a multi-channel sensor with different types of data
    info = pylsl.StreamInfo(
        name='TestSensor',
        type='Measurement',
        channel_count=4,
        nominal_srate=100,  # 100 Hz
        channel_format='float32',
        source_id='test_sensor_12345'
    )
    
    # Add channel descriptions
    channels = info.desc().append_child("channels")
    
    # Channel 1: Sine wave
    ch1 = channels.append_child("channel")
    ch1.append_child_value("label", "sine_wave")
    ch1.append_child_value("unit", "volts")
    ch1.append_child_value("type", "voltage")
    
    # Channel 2: Random values
    ch2 = channels.append_child("channel")
    ch2.append_child_value("label", "random_data")
    ch2.append_child_value("unit", "units")
    ch2.append_child_value("type", "misc")
    
    # Channel 3: Sawtooth wave
    ch3 = channels.append_child("channel")
    ch3.append_child_value("label", "sawtooth")
    ch3.append_child_value("unit", "percent")
    ch3.append_child_value("type", "percentage")
    
    # Channel 4: Square wave
    ch4 = channels.append_child("channel")
    ch4.append_child_value("label", "square_wave")
    ch4.append_child_value("unit", "state")
    ch4.append_child_value("type", "binary")
    
    # Add manufacturer info
    info.desc().append_child_value("manufacturer", "TestCompany")
    info.desc().append_child_value("model", "TestModel-1000")
    info.desc().append_child_value("serial_number", "SN123456")
    
    # Create outlet
    outlet = pylsl.StreamOutlet(info)
    
    print(f"Created LSL outlet: {info.name()} ({info.uid()})")
    print(f"Type: {info.type()}")
    print(f"Channels: {info.channel_count()}")
    print(f"Sample rate: {info.nominal_srate()} Hz")
    print("\nSending data... Press Ctrl+C to stop\n")
    
    # Send data
    start_time = time.time()
    sample_count = 0
    
    try:
        while True:
            # Calculate elapsed time
            elapsed = time.time() - start_time
            
            # Generate sample data
            sample = [
                math.sin(2 * math.pi * 1.0 * elapsed),  # 1 Hz sine wave
                random.uniform(-1, 1),                    # Random between -1 and 1
                (elapsed % 2.0) - 1.0,                   # Sawtooth from -1 to 1
                1.0 if int(elapsed) % 2 == 0 else -1.0  # Square wave
            ]
            
            # Send the sample
            outlet.push_sample(sample)
            
            sample_count += 1
            
            # Print status every 100 samples
            if sample_count % 100 == 0:
                print(f"Sent {sample_count} samples ({elapsed:.1f}s) - Current: {[f'{v:.3f}' for v in sample]}")
            
            # Sleep to maintain sample rate
            time.sleep(1.0 / info.nominal_srate())
            
    except KeyboardInterrupt:
        print(f"\nStopping... Sent {sample_count} samples total")

if __name__ == "__main__":
    main()