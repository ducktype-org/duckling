#!/usr/bin/env python3

import csv
import json
import sys
from dataclasses import dataclass, asdict
from enum import StrEnum
from typing import Dict, Optional, Any, Iterator, List

class Phase(StrEnum):
    COMPLETE = "X"
    INSTANT = "i"

class Category(StrEnum):
    GIL = "gil"
    SYNC = "sync"

class Scope(StrEnum):
    THREAD = "t"

class SyncPrimitive(StrEnum):
    START_THREAD = "StartThread"
    JOIN_THREAD = "JoinThread"
    CREATE_MUTEX = "CreateMutex"
    LOCK_MUTEX = "LockMutex"
    UNLOCK_MUTEX = "UnlockMutex"
    DESTROY_MUTEX = "DestroyMutex"
    CREATE_CV = "CreateCV"
    WAIT_CV = "WaitCV"
    NOTIFY_CV = "NotifyCV"
    NOTIFY_ALL_CV = "NotifyAllCV"
    DESTROY_CV = "DestroyCV"

@dataclass
class LogRecord:
    name: str
    thread_id: str

@dataclass
class TraceEvent:
    name: str
    cat: Category
    ph: Phase
    ts: int
    pid: int
    tid: str
    dur: Optional[int] = None
    args: Optional[Dict[str, Any]] = None
    s: Optional[Scope] = None

def clean_lines(file_object):
    for line in file_object:
        yield line.strip().rstrip(';')

def parse_log_to_records(log_filepath: str) -> Iterator[LogRecord]:
    with open(log_filepath, 'r', encoding='utf-8') as f:
        reader = csv.reader(clean_lines(f))
        for row in reader:
            if len(row) < 3:
                continue
            
            yield LogRecord(
                name=row[1].strip(),
                thread_id=row[2].strip()
            )

def process_records_to_trace(records: Iterator[LogRecord]) -> List[TraceEvent]:
    events: List[TraceEvent] = []
    current_thread: Optional[str] = None
    block_start_tick = 0
    ops_count = 0
    tick = 0

    valid_sync_primitives = {primitive.value for primitive in SyncPrimitive}

    for record in records:
        if record.thread_id != current_thread:
            if current_thread is not None:
                events.append(TraceEvent(
                    name=f"GIL Held ({ops_count} ops)",
                    cat=Category.GIL,
                    ph=Phase.COMPLETE,
                    ts=block_start_tick,
                    dur=ops_count,
                    pid=1,
                    tid=current_thread,
                    args={"operations_executed": ops_count}
                ))
                
            current_thread = record.thread_id
            block_start_tick = tick
            ops_count = 0
        
        if record.name in valid_sync_primitives:
            events.append(TraceEvent(
                name=record.name,
                cat=Category.SYNC,
                ph=Phase.INSTANT,
                ts=tick,
                pid=1,
                tid=current_thread,
                s=Scope.THREAD
            ))
        
        ops_count += 1
        tick += 1

    if current_thread is not None:
        events.append(TraceEvent(
            name=f"GIL Held ({ops_count} ops)",
            cat=Category.GIL,
            ph=Phase.COMPLETE,
            ts=block_start_tick,
            dur=ops_count,
            pid=1,
            tid=current_thread,
            args={"operations_executed": ops_count}
        ))
        
    return events

def convert_to_trace(log_filepath: str, json_filepath: str) -> None:
    try:
        records = parse_log_to_records(log_filepath)
        events = process_records_to_trace(records)

        serialized_events = [
            {k: v for k, v in asdict(e).items() if v is not None} 
            for e in events
        ]

        with open(json_filepath, 'w', encoding='utf-8') as f:
            json.dump(serialized_events, f, separators=(',', ':'))
            
        print(f"Successfully generated {json_filepath} with {len(events)} trace events.")

    except Exception as e:
        print(f"Error parsing log: {e}", file=sys.stderr)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.csv> <output.json>")
        sys.exit(1)
        
    convert_to_trace(sys.argv[1], sys.argv[2])
