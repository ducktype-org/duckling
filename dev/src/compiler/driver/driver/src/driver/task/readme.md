# Driver Tasks

A **task** is a single, self-contained job the compiler driver knows
how to execute. Tasks are independent of each other — each one carries
all the input data it needs and produces a well-defined result. The
driver receives a list of tasks, resolves them, and runs them.

Today there is one concrete kind of task: **package compilation**
(`PackageCompilationTask`). The
infrastructure is intentionally extensible — more task kinds can be
added in the future for any compiler job that takes structured input
and produces an artifact or side effect.

> Add a task here whenever you want the compiler to perform a specific
> piece of work that needs its own input data. Anything that fits the
> shape "given these parameters, do this" is a candidate — code
> generation variants, analyses with their own outputs, batched tooling
> jobs, etc.
