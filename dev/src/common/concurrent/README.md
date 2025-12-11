This is a module providing base components for concurrent/async programming.
It has similar feel to base-module, but unlike base it makes some high-level assumptions about its usage, mainly **it defines a "worker", and assumes that only a fixed, preset amount of workers are present in a system**. See more info about workers bellow.

## Workers

This module define so called **workers**.
Conceptually worker is essentially a thread that "performs" some work.
There are however some additional rules about workers:

* There can only be a fixed number of workers in the system, numbered from `0` to `N-1`, where `N` is the number of workers.

* The number of workers has to be set in the `module_flags` BEFORE the usage of the module. In release builds the module will panic if this will not be the case.

* Each worker has a uniquely assigned so called worker data – a global state predefined by the module. Some APIs of this module requires user to pass so called `WDRef` – a reference to worker data. Such references can be obtained with API exposed in the `concurrent/workers` subdirectory. Each WorkerData should only be used by a single worker. User of the model can technically also use the WorkerData for his own purposes, but it is usually inadvised.

* Technically a single worker can be executed by multiple threads, but the user of the module should ensure that computations performed by a given worker are strictly sequential. For example two threads might execute a single worker but only if it is guaranteed that any actions performed by the given two threads are mutually exclusive. A typical usage scenario is that a thread `A` that executed some worker `X`


This module does not provide any mechanism to actually manage workers (or threads). This should be performed by the user of this module.
Usually this will mean spawning a fixed amount of threads of providing each one with a unique `WDRef`.


## Rationale behind workers

In some cases a concept of worker can allow for significantly more performant implementation of some primitives/data structures.
This is the case for two main reasons:

* We can assume a limited concurrency expressed in the number of workers, preset by the user.

* Each worker data can store some global state that can be used by the module, that does not require any synchronization to access (mainly worker id and random number generator individual to each worker).

This way it possible to implement very efficient concurrent structures such as for example lock-free concurrent queue approximation or signal trees.

