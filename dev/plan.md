## Code review — Deadlock Detection

**Core problem:** detector state (`mutex_owners`, `thread_waiting_for_mutex`) is persistent process state but is only pruned on the three happy-path `mark*` calls + `clearMutexState`. Every abnormal path leaks stale edges. Combined with thread/mutex-id reuse (`StableObjectPool` free_ids), that breaks the "always acyclic" invariant the whole design rests on.

_(Line numbers are approximate — from the working tree.)_

### 🔴 Bugs

- **`deadlock_detection.cpp` `checkForDeadlock`** — the `while(true)` chain-walk has **no visited-set / hop-bound**; it terminates only if the graph is truly acyclic (per the comment). Any stale edge from the paths below can leave a cycle *not anchored* at the calling `thread_id` → the loop never hits `current == thread_id`, never reaches a chain end → **spins forever holding the GIL**, hanging the whole process. Add a visited set or a hop counter bounded by thread count — it's O(chain), essentially free.

- **`builtin_functions.cpp` lock slow-path** — sets `markThreadWaitingForMutex`, then can throw `KillProcessException` on terminate **without erasing the waiting edge**. With thread-id reuse, the next thread inherits a phantom "waiting for M" edge. Erase waiting state before throwing.

- **`deadlock_detection.cpp` `clearMutexState`** — erases only `mutex_owners`; the comment claims "an owned mutex cannot be destroyed" but nothing enforces it. `builtin_destroy_mutex` can run while a thread is blocked-waiting on M, leaving `thread_waiting_for_mutex[T]=M` dangling; mutex-id reuse then makes it a phantom edge to an unrelated mutex. Scan/clear waiting entries on destroy too.


PLAN:
1) read carefully what the bug states
2) read the code
3) come up with a slution'
4) implement it
5) lokk for the next bug ang go to step 1
