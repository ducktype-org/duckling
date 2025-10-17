# DVM - Supervisor module
## [`Supervisor`](./supervisor.hpp)
The `Supervisor` is the central, high-level management component of the virtual machine's architecture. 
It acts as the primary dispatcher, mediating all communication between the outside world (e.g., via an API) 
and the individual processes (`VMProcess`) running within the machine.

The `Supervisor` is the sole module responsible for creating and terminating virtual processes. It maintains 
a central registry of all active processes currently running within DVM. All external commands targeting 
a specific process (such as executing code or handling I/O) are first sent to the `Supervisor` and based on 
the Process ID (`PID`) included in the request, the `Supervisor` locates the corresponding `VMProcess` and forwards
the command for execution.