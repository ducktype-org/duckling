# Persistent data structures

This directory contains the persistent containers used by the VM. A *persistent* structure is one where an update does not destroy the previous version: instead, each operation returns a new state ID and the old states remain valid and queryable. Updates share as much of the previous state as possible, so keeping many versions around is cheap.

The containers are used by the debugger and by bytecode validation, both of which need to reason about many related states of the same data (stack states while walking a function, program states while injecting code).

## The containers

* [`Memory`](./memory.hpp) — a persistent indexed store, implemented as a persistent segment tree ([`tree.hpp`](./tree.hpp)). `MemoryStateID` identifies a state; two IDs are equal if and only if they represent equal memories. Used by `LocalStackDatabase`.
* [`Vector`](./vector.hpp) — a persistent vector. Its state is identified by a `VectorStateID`, and it is used to share prefixes between local stack states.
* [`HashMap`](./hashmap.hpp) — a persistent hash map.
* [`bijective_map.hpp`](../bijective_map.hpp) and [`stable_obj_id_name_map.hpp`](../stable_obj_id_name_map.hpp) — auxiliary maps that keep a bijection between names and IDs.
* [`dummy`](./dummy) — non-persistent stand-ins that implement the same interface for builds where the persistent versions are not needed.

## Dummy implementations

The `dummy/` directory provides a `hashmap` and a `vector` with the same API as the real ones, but without sharing. They are useful when persistence brings no benefit and the plain version is faster, and they keep the rest of the code from depending on the persistent implementation directly.

## Motivation for the library

`LocalStackDatabase` needed a way to keep a large number of stack states with shared prefixes without copying the whole stack each time, and the same idea was reused for the program state in the validator. The [`persistent_structures_test.cpp`](../../../../tests/utils/persistent_structures_test.cpp) file covers the `Memory`, `Vector` and `HashMap` implementations.
