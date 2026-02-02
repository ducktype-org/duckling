# Race Tester Module

A comprehensive framework for testing the correctness of concurrent data structures through **linearizability verification**. This module records concurrent operation histories and validates them against a sequential reference implementation using breadth-first search.

## Overview

The Race Tester validates concurrent data structures by:
1. **Recording** concurrent operations as they execute across multiple threads
2. **Verifying** that the observed behavior is equivalent to some sequential execution (linearizability)

This is based on the formal concept of **linearizability** (probably explained during your favourite undergraduate concurrent programming course), which means that concurrent operations **appear to execute atomically at some point between their invocation and completion**.

See [](#architecture) to understand roughly what the hell is going on, and [](#usage) for practical usage instructions.

## Architecture

The module consists of five key components that work together. The main API is provided by the `RaceTester` class. However, understanding the other components may help in understanding why the API is somewhat… inconvenient.

Before reading further, note that it is important to distinguish between several concepts which are used precisely in this README:
- **Tested Implementation:** The concurrent data structure implementation under test (e.g., a lock-free stack that you are not sure is correct)
- **Sequential Implementation:** A known-correct (reference), sequential version of the data structure (e.g. std::stack)
- **Tested Interface:** The common interface that both implementations satisfy (e.g., `StackInterface` with `push()` and `pop()` methods). It may be necessary for you to create thin adapter classes to adapt existing implementations to the common interface.

### 1. `RaceTester` (Main API)
This is the top-level orchestrator that ties all components together. It:
- Manages the tested concurrent implementation and the sequential reference implementation
- Provides the `run()` method to execute concurrent workloads and record their history
- Provides the `check()` method to verify linearizability
- Combines both operations in `runAndCheck()` for convenience

**Template Parameters:**
- `TestedInterface` — The common interface both implementations satisfy
- `TestedImplementation` — The concurrent implementation under test
- `SequentialImplementation` — The sequential reference implementation (must be copy-constructible)
- `PossibleResults...` — All possible return types from operations (use `std::monostate` for void). Note that for the purpose of outputting a non-linearizable history, all result types must be handled by `strConcat`. This seems cumbersome, but not to worry, you'll only need to specify this once, and then you can forget about it.

### 2. `History`
The history provides a sequential view of all concurrent interactions, which is the input to the verification algorithm.

It is a thread-safe container that holds the complete timeline of concurrent events:
- **Call events** — Record when an operation begins, including its description and the function object representing the operation, so that it can be later **replayed on the sequential implementation**
- **Return events** — Record when an operation completes, including its result (hence the necessity for the `PossibleResults` template parameters)
- Each event is tagged with a thread ID

Note that the History only holds the data about the operations, it does not record them. The recording is done by the `Executor`, described below.

### 3. `Executor`
The Executor is a "security checkpoint" between the worker threads and the **tested implementation**. The motivation for its existence is as follows.

Ordinarily, a worker thread would directly call methods on the tested implementation. However, this would allow the operations to execute without being recorded, or being recorded incorrectly (e.g. with the wrong thread ID). The executor acts "in the name of" a thread and holds the threads ID. It intercepts all calls to the tested implementation via the `execute` method, and ensures that they are properly recorded in the History.

In greater detail, instead of a worker calling a method of the tested interface directly, it passes **a function object representing the operation** to the executor's `execute` method, along with a description string.

The worker-facing API for executing and recording operations:
- Workers call `execute(description, operation)` to perform an operation
- Automatically records both the call and return events in the history
- Ensures proper sequencing of events (call before execution, return after)

### 4. `Coordinator`
Overall a simple class. It manages the lifecycle of worker threads during test execution:
- Spawns multiple worker threads according to the specified count
- Provides each worker with its unique ID and a corresponding `Executor` instance
- Ensures all threads complete before returning control

The worker function receives the thread ID **only** so that it can assume a specific role in a test, if desired. Typically, this is not necessary, but an example is available in the `historyTest` method in [race_tester_test.cpp](../../../tests/race_tester_test.cpp).

### 5. `BFSLinearizer`
The verification engine that determines if a recorded history is linearizable.

It uses the operation function objects recorded in the History to replay those operations on the **sequential implementation** in various orders, searching for a valid linearization. Note that for a linearization to be valid, the results of the operations must match those recorded in the history.

**Algorithm:**
1. Starts with the initial sequential state and all threads' first operations
2. Uses breadth-first search to explore all possible linearization orders
3. For each state, tries advancing each thread's next operation (provided that it begins before any other thread's next operation returns)
4. Validates that the operation's result matches the recorded history
5. Continues until all operations are linearized or no valid ordering exists

By trying all possible orderings of concurrent operations, the BFS explores the entire space of valid sequential executions. If any ordering produces results matching the recorded history, the concurrent execution is linearizable (correct). Note that the search space might grow exponentially with the number of concurrent operations, so the tests should be kept reasonably small, and designed to aid the framework in rejecting search paths early. See [](#usage).

The BFS approach finds the shortest counterexample if the implementation is incorrect, which aids debugging.

Note that the BFSLinearizer requires the sequential implementation to be copy-constructible, as it needs to maintain multiple independent copies of the state during exploration.

## Usage

For a practical example, see the `linearizationTest` method in [race_tester_test.cpp](../../../tests/race_tester_test.cpp).

### Basic Usage Pattern

#### 1. Define your concurrent data structure interface

Note the use of `std::monostate` for operations that would otherwise return `void`.

```cpp
class QueueInterface {
public:
    virtual std::monostate enqueue(int value) = 0;
    virtual std::optional<int> dequeue() = 0;
    virtual ~QueueInterface() = default;
};
```

#### 2. Implement the tested and reference versions

```cpp
class LockFreeQueue : public QueueInterface {
    // ... your lock-free implementation ...
};

class SequentialQueue : public QueueInterface {
    // ... your guaranteed-to-be-correct sequential implementation ...
};
```

#### 3. Set up the race tester
```cpp
auto tested = makeBox<LockFreeQueue>();
auto sequential = makeBox<SequentialQueue>();

using Tester = concurrent::tester::RaceTester<
    QueueInterface,
    LockFreeQueue,
    SequentialQueue,
    std::monostate,        // enqueue returns void (use monostate)
    std::optional<int>     // dequeue returns optional<int>
>;

Tester tester{tested.refMut(), sequential.ref()};
```

#### 4. Define the worker function

This is easily the most annoying part of the test, in no small part because of the API of the tester. It requires you to express each operation as a function object, which is a bit cumbersome, but is made worse by the need to pass them to a method of an argument of a larger function object. It's lambda-ception!

The worker function typically performs a random mix of operations as quickly as possible to maximize concurrency.

Remember that the worker takes:
- `u32 thread_id` — The unique ID of the worker thread (only so that it can assume a specific role, if desired)
- `Tester::Executor_ executor` — The executor instance for this thread, used to actually perform and record operations

**Hint:** Keep in mind that it is preferable to:
- keep the total number of operations small to avoid search space blow-up
- return meaningful results that can help the linearizer prune invalid paths early
- keep the size of the sequential implementation small for efficient copying (for example have 60% of the operations be dequeues, and 40% enqueues)

```cpp
auto worker = [](u32 thread_id, Tester::Executor_ executor) {
    for (int i = 0; i < 100; ++i) {
        if (random.nextDouble() < 0.4) {
            // Enqueue a random value
            int value = random.nextInt(0, 100);
            executor.execute(
                "enqueue(" + std::to_string(value) + ")",
                [value](Ref<QueueInterface> q) {
                    return q->enqueue(value);
                }
            );
        } else {
            // Try to dequeue
            executor.execute(
                "dequeue()",
                [](Ref<QueueInterface> q) { return q->dequeue(); }
            );
        }
    }
};
```

#### 5. Run the test with multiple workers and check linearizability

```cpp
bool is_correct = tester.runAndCheck(4, worker);

if (is_correct) {
    std::cout << "Implementation is linearizable (correct)!\n";
} else {
    std::cout << "Implementation has a race bug!\n";
}
```

If the test fails, the history with the counterexample is printed to `stderr` automatically by `runAndCheck`.

**Hint:** The test is inherently non-deterministic. To gain confidence in correctness, consider running the test multiple times (e.g., 10-100 repetitions).

### Advanced Usage: Separate Run and Check

For more control, you can separate the execution and verification phases:

```cpp
// Run the concurrent test
tester.run(4, worker);

// Inspect the history
std::cout << tester.getHistory()->toString() << "\n";

// Verify linearizability
if (!tester.check()) {
    std::cerr << "Found a counterexample!\n";
}
```

Although I haven't really found a use case for this yet, other than the `historyTest`.

## Some Design Rationale

**Why function objects in History?**
Operations are stored as function objects to enable replaying them on the sequential implementation during linearization checking. This allows the same recorded operation to be executed on different state copies.

**Why use Executors?**
The design of the tester module enforces (via appropriate types) that a worker cannot influence the tested implementation without interacting with its dedicated executor. This ensures that all operations are properly recorded in the history.

**Why BFS instead of DFS?**
BFS naturally finds the shortest counterexample, which is easier to understand and debug. It also provides an intuitive (as can be) non-recursive implementation, which should be preferred in search problems such as this one.

**Why require copy-constructible sequential implementation?**
The BFS explores multiple states simultaneously. Each state requires an independent copy of the sequential implementation. Another alternative would be to implement a **reversible** sequential implementation that can undo operations, but this would complicate the design significantly (both the in terms of implementation and usage).

## Credits

This module was developed by Maurycy Wojda, based loosely on the (much more complicated) Concurrent Algorithms and Data Structures (CADS) Scala library of Oxford University.
