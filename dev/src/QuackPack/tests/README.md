### Tests for QuackPack

Structure and scope:

- `unit/`: fast unit tests for utilities, manifest parsing, fetcher clients, identifiers, versions, etc.
- `signals/`: tests for robust signal handling contexts and behavior.
- `filelock/`: cross-platform file lock behavior (POSIX/Windows/software) including interrupt handling.
- `solver/`: scenario-based tests for solver phases and registry resolution.
