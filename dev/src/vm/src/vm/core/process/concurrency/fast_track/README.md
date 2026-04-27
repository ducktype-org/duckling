# FastTrack Concurrency Components

This directory contains the core data structures for the **FastTrack** dynamic race detection algorithm.

## Contents

- **`epoch.hpp` / `epoch.cpp`**: Implements the `Epoch` class, representing $c@t$ (clock $c$ for thread $t$).
- **`vc.hpp` / `vc.cpp`**: Implements the `VectorClock` class ($VC$), providing pointwise operations and join functionalities.
- **`CMakeLists.txt`**: Integration with the DVM build system.

## Mathematical Definitions

The implementation follows the definitions from *FastTrack: Efficient and Precise Dynamic Race Detection* (Flanagan & Freund):

- **Join**: $VC \sqcup c@t \implies VC[t] = \max(VC[t], c)$
- **Happens-Before (Epoch-VC)**: $c@t \sqsubseteq VC \iff c \le VC(t)$
- **Happens-Before (Epoch-Epoch)**: $c@t \sqsubseteq c'@t' \iff t = t' \land c \le c'$
- **Happens-Before (VC-VC)**: $VC_1 \sqsubseteq VC_2 \iff \forall t. VC_1(t) \le VC_2(t)$
