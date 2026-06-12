# VM API torture tests

Stress-tests the DVM through its public API (`vm/api/vm.hpp`), inspired by
curl's torture tests: systematic re-execution with injected adversity and
strict oracles, instead of hand-written happy paths.

## Phase 1 (implemented here): model-based API stress

`api_torture_tests.cpp` runs seeded, randomized API call sequences against
processes executing small `.dbc` programs:

- **terminating program** (`breakpoint.dbc`): run to completion under a barrage
  of debugger/status requests, then join and validate,
- **infinite program** (`while_true.dbc`): random `pause`/`resume`/`step`/
  breakpoint toggles/status reads, then stop and kill,
- **concurrent clients**: several threads hammer the same process with the
  random op mix while the program runs.

Oracles:

1. **Lifecycle model**: a status listener records every emitted `ProcStatus`;
   every consecutive pair must be a single-event edge of
   `vm::lifecycle::statusTransitions()`. In debug builds the VM additionally
   self-checks: a thread dispatching an event its machine rejects panics.
2. **No hangs**: a watchdog aborts the suite (printing the seed) if a scenario
   exceeds its time budget.
3. **No crashes**: any signal/abort fails the test run.
4. **Guest memory**: terminating scenarios end with `deinitAndValidate`, which
   validates the *guest program's* memory state. Note: this does NOT cover
   VM-internal (host-side) leaks - see phase 3 notes.
5. **API sanity**: calls may fail (racing a completing program is legal), but
   must return a well-formed result; calls with guaranteed semantics
   (`getExecutionStatus`) must succeed.

Reproducibility: the master seed is printed at the start of every test and on
watchdog timeout. Override with `DUCK_TORTURE_SEED=<n>`; scale the number of
scenarios with `DUCK_TORTURE_ITERATIONS=<n>` (default keeps CI fast; nightly
runs can crank it up).

Deliberately excluded from the random op mix (for now):

- `waitForBreakpoint` - blocks unboundedly when nothing will pause; covered by
  the debugger tests,
- `input`/`output` - `output()` blocks while the program is executing; needs a
  dedicated IO-program scenario,
- `runFunctionAwait` - needs argument plumbing per function signature.

## Phase 2 (planned): schedule perturbation

Debug-build hook injecting random `yield`/short `sleep` inside
`state_machine::WaitableStateMachine` dispatch and the status emitter, to widen
race windows. The lost-wakeup and mid-dispatch entry races fixed in #2922 are
exactly the class of bug this multiplies coverage for.

## Phase 3 (planned, optional): allocation torture

curl-style fault injection: count fallible allocations in a scenario, then
re-run failing each one in turn, asserting graceful errors and no leaks.
Start with the VM's own `Memory` module (guest OOM paths). Host-side `malloc`
torture only if host OOM-safety becomes a contract.

**VM-internal leak detection** is not covered by `deinitAndValidate` (guest
memory only); it should come from an ASan/LSan (or valgrind) nightly job
running this suite.
