# Concurrency and Synchronization

This module manages concurrent execution within a DVM process, implementing thread safety mechanisms and synchronization primitives.

## Global Interpreter Lock (GIL)

The `GIL` (defined in `gil.hpp` and `gil.cpp`) is a process-wide lock that ensures only one thread can execute DVM bytecode at any given time. This simplifies the interpreter implementation and memory management by avoiding complex concurrent access patterns.

### GIL Architecture

- **Mutual Exclusion:** The GIL uses a `std::timed_mutex` to enforce that only one thread holds execution rights at a time.
- **Exchange Counter:** Tracks how many times the GIL has been transferred between threads, allowing waiting threads to detect when the GIL holder has released it.
- **Release Policy:** Implements a timeout-based policy where threads waiting for the GIL set a flag after 5 milliseconds, signaling the current holder to release it voluntarily.

### GIL Methods

- `acquire()`: Acquires the GIL. If unavailable, waits with a timeout and signals the holding thread if contention is detected.
- `release()`: Releases the GIL and increments the exchange counter.
- `shouldRelease()`: Checks if the current thread should release the GIL based on the contention flag.

## Thread Integration

Each `VMThread` has a `has_gil` flag indicating whether it currently holds the GIL. The thread acquires the GIL before executing DVM instructions and may release it periodically based on the GIL release policy.

### VMThread GIL Operations

- `keepOrAcquireGil()`: Called at each instruction execution point. Checks the release policy and either keeps the GIL or releases and reacquires it.
- `releaseGil()`: Explicitly releases the GIL (used when calling external functions that don't need the GIL).

## Synchronization Primitives

The `SynchronizationPrimitives` class provides high-level synchronization constructs for DVM programs:

### Mutexes

- `createMutex()`: Creates a new mutex and returns its ID.
- `getMutex(id)`: Retrieves a mutex by ID.
- `removeMutex(id)`: Destroys a mutex.

Mutexes are stored in a hash map and are accessible across threads in the same process.

## Built-in Threading Functions

The VM provides built-in functions for thread management:

- `builtin_start_thread`: Creates and starts a new thread executing a specified function.
- `builtin_join_thread`: Waits for a thread to complete.
- `builtin_create_mutex`: Creates a new synchronization mutex.
- `builtin_lock_mutex`: Acquires a mutex.
- `builtin_unlock_mutex`: Releases a mutex.
- `builtin_destroy_mutex`: Destroys a mutex. Panics if the mutex is still locked (a program error).

## Notes

- GIL release policy is configurable through the timeout value (currently 5ms).
