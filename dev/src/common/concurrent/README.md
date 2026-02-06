# Concurrent

This is a module providing components for concurrent/async programming.
It has similar feel to base-module, but unlike base it makes some high-level assumptions about its usage, mainly **it defines *Workers*, and assumes that only a fixed, preset amount of workers are present in the system**. See more info about workers below.

## Workers

Workers are essentially just threads that are concurrently using this module at any given point in time (or, more generally, are present in the system at any given point in time).
Additionally there are few important rules about workers:

* There is a fixed number of workers in the system. This number has to be set in `module_flags` before using certain functionalities of this module. In Dev builds this module will panic, if the number is not set.
* Workers are managed by the *WorkerManager* singleton, which owns the workers. Workers can be easily accessed by the thread-safe, public API of the *WorkerManager*.
* Each *Worker* has a thread-safe, public API.

This allows implementation of some concurrent algorithm to be much simpler or more efficient. This is mainly due to the limited concurrency expressed in the maximum number of workers and the worker indexes and per-worker rng provided by *Worker*'s API.

It is allowed to use the idea of the *Worker* outside of this module (intended mostly for purposes of query concurrent execution).
