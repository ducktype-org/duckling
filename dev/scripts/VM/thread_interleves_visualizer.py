#!/usr/bin/env python3

import csv
import json
import sys

def clean_lines(file_object):
    for line in file_object:
        yield line.strip().rstrip(';')

def convert_to_trace(log_filepath, json_filepath):
    events = []
    current_thread = None
    block_start_tick = 0
    ops_count = 0
    tick = 0

    sync_primitives = {
        "StartThread", "JoinThread", "CreateMutex", "LockMutex",
        "UnlockMutex", "DestroyMutex", "CreateCV", "WaitCV",
        "NotifyCV", "NotifyAllCV", "DestroyCV"
    }

    try:
        with open(log_filepath, 'r', encoding='utf-8') as f:
            reader = csv.reader(clean_lines(f))
            for row in reader:
                if len(row) < 3:
                    continue
                
                name = row[1].strip()
                thread_id = row[2].strip()
                
                if thread_id != current_thread:
                    if current_thread is not None:
                        events.append({
                            "name": f"GIL Held ({ops_count} ops)",
                            "cat": "gil",
                            "ph": "X",
                            "ts": block_start_tick,
                            "dur": ops_count,
                            "pid": 1,
                            "tid": current_thread,
                            "args": {"operations_executed": ops_count}
                        })
                    current_thread = thread_id
                    block_start_tick = tick
                    ops_count = 0
                
                if name in sync_primitives:
                    events.append({
                        "name": name,
                        "cat": "sync",
                        "ph": "i",
                        "ts": tick,
                        "pid": 1,
                        "tid": current_thread,
                        "s": "t"
                    })
                
                ops_count += 1
                tick += 1

            if current_thread is not None:
                events.append({
                    "name": f"GIL Held ({ops_count} ops)",
                    "cat": "gil",
                    "ph": "X",
                    "ts": block_start_tick,
                    "dur": ops_count,
                    "pid": 1,
                    "tid": current_thread,
                    "args": {"operations_executed": ops_count}
                })

        with open(json_filepath, 'w', encoding='utf-8') as f:
            json.dump(events, f, separators=(',', ':'))
            
        print(f"Successfully generated {json_filepath} with {len(events)} trace events.")

    except Exception as e:
        print(f"Error parsing log: {e}", file=sys.stderr)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python thread_interleaves_visualizer.py <input.csv> <output.json>")
        sys.exit(1)
        
    convert_to_trace(sys.argv[1], sys.argv[2])
