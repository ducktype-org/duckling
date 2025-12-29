This is a module providing base components for concurrent/async programming.
It has similar feel to base-module, but unlike base it makes some high-level assumptions about its usage, mainly **it defines a "worker", and assumes that only a fixed, preset amount of workers are present in the system**. See more info about workers bellow.

## Workers

This module defines so called **workers**.
Workers are essentially just threads that are concurrently using this module at any given point in time (or, more generally, are present in the system at any given point in time).
Additionally there are few important rules about workers:

* There is a fixed number of workers in the system. This number has to be set in `module_flags` before using certain functionalities of this module. In Dev builds this module will panic, if the number is not set.

* There exists so called `WorkerData`. This is just a data defined by this module that is provided for each worker and can be obtained with API exposed in the `concurrent/workers` subdirectory. Some APIs of this module require the user to pass so called `WDRef` – a reference to worker data as part of their interface. Crucial aspect of a `WorkerData` it that, since it is unique to each worker, it can be accessed without any synchronization. Note that it might be more efficient to store `WDRef` locally, then to acquire it each time it is needed.

Those two assumptions allow implementation of some concurrent algorithm to be much simpler or more efficient. This is mainly due to the limited concurrency expressed in the maximum number of workers and the worker indexes and per-worker rng provided by the `WorkerData`.


It is allowed to use the idea of the worker and the `WorkerData` outside this module (intended mostly for purposes of query concurrent execution).

