# FastTrack Concurrency Components

This directory contains the core data structures for the **FastTrack** dynamic race detection algorithm.

## Contents

- **`epoch.hpp` / `epoch.cpp`**: Implements the `Epoch` class, representing $c@t$ (clock $c$ for thread $t$) packed into one 64-bit word.
- **`vc.hpp` / `vc.cpp`**: Implements the `VectorClock` class ($VC$), providing pointwise operations and join functionalities.
- **`shadow_entry.hpp` / `shadow_entry.cpp`**: Implements `ShadowEntry`, the FastTrack state of one memory location, with the read and write rules.
- **`shadow_memory.hpp`**: The shadow flavour of the memory module: `GenericMemory<ShadowEntry>` and its block type.
- **`fast_track_thread_stack.hpp`**: The per-thread shadow stack and its frames.
- **`fast_track_thread_data.hpp`**: Per-thread state: the thread's vector clock and its shadow stack pointers.
- **`fast_track_globals.hpp` / `fast_track_globals.cpp`**: Process-wide state: the shadow memory, the global shadow buffer and the block-to-shadow map.
- **`CMakeLists.txt`**: Integration with the DVM build system.

The engine is not wired into instruction execution yet; that lands with #3559.

## Mathematical Definitions

The implementation follows the definitions from *FastTrack: Efficient and Precise Dynamic Race Detection* (Flanagan & Freund):

- **Join**: $VC \sqcup c@t \implies VC[t] = \max(VC[t], c)$
- **Happens-Before (Epoch-VC)**: $c@t \sqsubseteq VC \iff c \le VC(t)$
- **Happens-Before (VC-VC)**: $VC_1 \sqsubseteq VC_2 \iff \forall t. VC_1(t) \le VC_2(t)$
