# Deadlock Detection in the DVM

## What is a deadlock?

A deadlock occurs when two or more threads are each waiting for a mutex held by the other, so none of them can ever make progress. Example:

- Thread A holds mutex 1 and waits for mutex 2.
- Thread B holds mutex 2 and waits for mutex 1.

Both threads are permanently blocked.

## How detection works

The `DeadlockDetector` class (in `src/vm/src/vm/core/safe/concurrency/deadlock_detection.hpp`) maintains a **wait-for graph** — a directed graph where an edge from thread T to thread T' means "T is waiting for a mutex currently held by T'".

### Graph structure

Because each thread can wait for at most one mutex at a time and each mutex is owned by at most one thread, the graph is a **functional graph**: every node has out-degree ≤ 1. This is a key property — it guarantees that following the chain of edges always terminates.

Two internal maps maintain the graph state (both protected by the process GIL):

| Map | Meaning |
|-----|---------|
| `thread_waiting_for_mutex` | thread → mutex it is waiting for |
| `mutex_owners`             | mutex → thread currently holding it |

### Detection algorithm

Deadlock detection runs inside `beginWaitForMutexOrThrow`, which is called by `builtinLockMutex` **before** the OS-level lock is acquired (while the GIL is still held):

1. Look up who owns the target mutex.
2. Follow the chain: owner → what mutex is that thread waiting for → who owns that → ...
3. If at any point the chain leads back to the calling thread, a cycle exists → throw `VMDeadlockException`.
4. If the chain ends without a cycle (a thread is not waiting, or a mutex is free), no deadlock → continue.

Because the existing graph is always acyclic (maintained by this check), following the chain is guaranteed to terminate without needing visited-set tracking.

### When the graph is updated

| Event | Method called |
|-------|--------------|
| Thread about to wait for mutex | `beginWaitForMutexOrThrow` → checks + `markThreadWaitingForMutex` |
| Thread successfully acquired mutex | `markThreadAcquiredMutex` (removes "waiting", records "owner") |
| Thread released mutex | `markThreadReleasedMutex` |
| Mutex is destroyed | `clearMutexState` |

All of these run with the process GIL held, so there is no internal synchronization inside `DeadlockDetector`.

### Where it lives

```
SafeVMProcess
└── DeadlockDetector deadlock_detector   (one per process)

SafeVMThread
└── SafeVMProcess& safe_process          (thread holds a reference to process)
    └── safe_process.getDeadlockDetector()  (used inside builtinLockMutex)
```

## Enabling and disabling detection

Detection is **disabled by default**. You must explicitly pass `true` to `vm::api::spawn()` to turn it on:

```cpp
// Without detection (default)
auto result = vm::api::spawn();

// With detection
auto result = vm::api::spawn(true);
```

### How the flag propagates

```
spawn(bool enable_deadlock_detection)
  └── Supervisor::newProcess(enable_deadlock_detection)
        └── new SafeVMProcess(pid, enable_deadlock_detection)
              └── deadlock_detector.setEnabled(enable_deadlock_detection)
```

### What "disabled" means

When `detection_enabled == false`:

- `checkForDeadlock` is **not called** → no exception is thrown.
- `markThreadWaitingForMutex` is still called → the graph stays consistent in case you check it later.
- If a real deadlock occurs, the involved threads will loop indefinitely retrying `try_lock_for` (500 ms timeout per attempt) and checking `isTerminateRequested()`. They will only stop when the process is killed externally.

### Typical use cases

- **Disable (default):** Normal operation — no overhead from graph maintenance.
- **Enable:** Pass `spawn(true)` when you want runtime deadlock detection. Deadlocks are caught immediately with a descriptive error, which is useful during development and testing of concurrent code.

## Exception thrown on detection

```
vm::exceptions::VMDeadlockException
```

The message is `VMDeadlockException::ERR_MSG` ("Deadlock detected"). The VM treats this as a runtime panic: the panicking thread's status becomes `ExecutionPanicked`, and the process auto-stop logic terminates all sibling threads.
