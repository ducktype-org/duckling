### Solver tests

Scenario-style unit tests for solver phases and registry resolution:

## `phases_unit/phases_4_and_5/test_1_S_registry.py`
Constructs small graphs with feature flags and validates chosen versions, flags to install, and build instructions for registry/local packages.

## `phases_unit/phases_4_and_5/test_2_M_registry.py`
Medium graph scenario with multiple alternatives and feature conditions across packages; asserts final instructions and flags.
