# Deadlock Detection

## The Problem

When multiple threads acquire mutexes in different orders, they can end up waiting for each other in a cycle — a deadlock. The VM detects this proactively and throws `VMDeadlockException` rather than hanging.

## Graph Model

The algorithm models the situation as a **wait-for graph**:

```
Thread A --waits for--> Mutex M --owned by--> Thread B
```

A deadlock exists if and only if there is a **cycle** in this graph.

Two maps maintain the state:
- `thread_waiting_for_mutex`: thread → mutex it's currently blocked on
- `mutex_owners`: mutex → thread currently holding it

Composing the two maps gives a direct **thread → thread** relationship (who is blocking whom). Since each thread waits for at most one mutex at a time, and each mutex is held by at most one thread, every thread has **out-degree ≤ 1** in this graph. This makes it a *functional graph* — every node leads down a single chain.

## The Core Idea

**The existing graph is always acyclic** — `checkForDeadlock` prevents any cycle from being formed before it is committed.

This means that when checking whether adding a new wait-edge `(thread_id → owner)` would create a cycle, we just follow the chain forward from `owner`:

```
owner → (who owner waits for) → (who that thread waits for) → ...
```

- If we reach `thread_id`, adding the edge would close a cycle → **deadlock**, throw.
- If we reach a thread that isn't waiting for anything, the chain ends → **safe**, return.

Since the graph has no existing cycles, the chain is guaranteed to terminate — no visited tracking or recursion stack is needed.

## Why No Visited Set?

Earlier versions used a DFS with a `visited` vector and a `stack` to avoid infinite loops. But since each thread has at most one outgoing edge and the graph is always acyclic, the chain can never loop back on itself. The visited tracking was protecting against a case that cannot occur.

Removing it eliminates two heap allocations on every `checkForDeadlock` call.

## Complexity

- **Time:** O(N) where N = number of threads in the wait-for chain (at most total thread count).
- **Space:** O(1) extra — just a pointer walking the chain.

For typical programs with a small number of threads, the chain is short and the check is essentially instant.
