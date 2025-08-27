### quackpack.core.solver

Dependency solving pipeline and build rules construction.

## `solver.py`
Façade entry for solver-related operations.

## `util.py`
Small helpers shared across solver subpackages.

### Subpackages
- `gathering/`: types and routines to gather summaries and resolvents before solving.
- `solving/`: solver engine, model and construction of build instructions.
- `cleaning/`: cleanup passes for unusable or conflicting items.
- `types/`: representation of resolved/unresolved IDs and packages, flags, and maps.
