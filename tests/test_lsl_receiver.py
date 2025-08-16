#!/usr/bin/env python3
"""
LSL Test Receiver - Displays all available LSL streams and their data
This script discovers all LSL streams and displays their incoming data.
"""

import pylsl
import time
import threading
from collections import defaultdict

class LSLReceiver:
    def __init__(self):
        self.inlets = {}
        self.stream_data = defaultdict(lambda: {'last_sample': None, 'sample_count': 0})
        self.running = True
        
    def discover_streams(self):
        """Discover all available LSL streams"""
        print("Discovering LSL streams...")
        streams = pylsl.resolve_streams()
        
        if not streams:
            print("No LSL streams found!")
            return
        
        print(f"\nFound {len(streams)} streams:")
        for i, info in enumerate(streams):
            print(f"\n[{i+1}] Stream: {info.name()}")
            print(f"    Type: {info.type()}")
            print(f"    UID: {info.uid()}")
            print(f"    Channels: {info.channel_count()}")
            print(f"    Sample rate: {info.nominal_srate()} Hz")
            print(f"    Format: {info.channel_format()}")
            print(f"    Source ID: {info.source_id()}")
            print(f"    Hostname: {info.hostname()}")
            
            # Try to get channel info
            desc = info.desc()
            channels = desc.child("channels")
            if channels:
                print("    Channel details:")
                ch = channels.child("channel")
                ch_idx = 0
                while ch:
                    label = ch.child_value("label") or f"ch{ch_idx+1}"
                    unit = ch.child_value("unit") or "unknown"
                    print(f"      [{ch_idx}] {label} ({unit})")
                    ch = ch.next_sibling("channel")
                    ch_idx += 1
        
        return streams
    
    def create_inlet(self, stream_info):
        """Create an inlet for a stream"""
        uid = stream_info.uid()
        if uid not in self.inlets:
            inlet = pylsl.StreamInlet(stream_info)
            self.inlets[uid] = {
                'inlet': inlet,
                'info': stream_info,
                'thread': None
            }
            print(f"\nCreated inlet for: {stream_info.name()} ({uid})")
    
    def receive_data(self, uid):
        """Receive data from a specific inlet"""
        inlet_data = self.inlets[uid]
        inlet = inlet_data['inlet']
        info = inlet_data['info']
        
        while self.running:
            try:
                # Pull sample with timeout
                sample, timestamp = inlet.pull_sample(timeout=0.1)
                if sample:
                    self.stream_data[uid]['last_sample'] = sample
                    self.stream_data[uid]['sample_count'] += 1
                    self.stream_data[uid]['timestamp'] = timestamp
            except Exception as e:
                print(f"Error receiving from {info.name()}: {e}")
                break
    
    def start_receiving(self):
        """Start receiving threads for all inlets"""
        for uid, inlet_data in self.inlets.items():
            thread = threading.Thread(target=self.receive_data, args=(uid,))
            thread.daemon = True
            thread.start()
            inlet_data['thread'] = thread
    
    def display_data(self):
        """Display received data in a formatted way"""
        try:
            while self.running:
                # Clear screen (works on most terminals)
                print("\033[2J\033[H", end='')
                
                print("LSL Stream Monitor - Press Ctrl+C to stop")
                print("=" * 80)
                
                if not self.inlets:
                    print("No streams connected!")
                else:
                    for uid, inlet_data in self.inlets.items():
                        info = inlet_data['info']
                        data = self.stream_data[uid]
                        
                        print(f"\n{info.name()} ({info.type()})")
                        print(f"  UID: {uid}")
                        print(f"  Samples received: {data['sample_count']}")
                        
                        if data['last_sample']:
                            print(f"  Latest sample:")
                            # Get channel labels if available
                            desc = info.desc()
                            channels = desc.child("channels")
                            
                            for i, value in enumerate(data['last_sample']):
                                label = f"ch{i+1}"
                                unit = ""
                                
                                if channels:
                                    ch = channels.child("channel")
                                    for j in range(i+1):
                                        if j == i and ch:
                                            label = ch.child_value("label") or label
                                            unit = ch.child_value("unit") or ""
                                        if ch:
                                            ch = ch.next_sibling("channel")
                                
                                unit_str = f" {unit}" if unit else ""
                                print(f"    [{i}] {label}: {value:.6f}{unit_str}")
                
                print("\n" + "=" * 80)
                time.sleep(0.1)  # Update display 10 times per second
                
        except KeyboardInterrupt:
            pass
    
    def run(self):
        """Main run loop"""
        # Discover streams
        streams = self.discover_streams()
        
        if not streams:
            print("\nNo streams found. Make sure LSL senders are running!")
            return
        
        # Ask user which streams to connect to
        print("\nEnter stream numbers to connect (comma-separated, or 'all'):")
        choice = input("> ").strip()
        
        if choice.lower() == 'all':
            selected_streams = streams
        else:
            try:
                indices = [int(x.strip()) - 1 for x in choice.split(',')]
                selected_streams = [streams[i] for i in indices if 0 <= i < len(streams)]
            except:
                print("Invalid input! Connecting to all streams.")
                selected_streams = streams
        
        # Create inlets for selected streams
        for stream in selected_streams:
            self.create_inlet(stream)
        
        # Start receiving threads
        self.start_receiving()
        
        # Display data
        print("\nStarting data display...")
        time.sleep(1)
        self.display_data()
        
        # Cleanup
        self.running = False
        print("\nShutting down...")

def main():
    receiver = LSLReceiver()
    try:
        receiver.run()
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()