# Frontend Packages

Resolved package metadata used by the rest of the frontend: package id,
import name, version, feature flags and the list of direct dependencies
(with their aliases). Built from manifest input (`RawPackageInfo`) and
stored in global compiler state, then accessed from queries through
`*AccessLocked` wrappers that participate in the incremental query
framework.

## Why locks (`*AccessLocked`)

The compiler is **query-based and incremental**. Anything a query reads
must be turned into a tracked dependency, so that when a value changes
the query is invalidated and re-run on the next build. For packages, the
inputs that can change between builds are:

- whether a given package exists at all (id + identity),
- the **size** of its dependency list (something added/removed),
- a specific `(package, alias) → target package id` lookup result
  (alias added, removed, or repointed).

> `version` and `features` are **not tracked** currently (see #2668) —
> generated code may eventually depend on them (feature-gated APIs,
> version conditionals), at which point reads will need to register the
> dependency.

The relevant side inputs:

| AccessLocked                               | Side input registered on `unlock`            |
|--------------------------------------------|-----------------------------------------------|
| `PackageAccessLocked`                      | `QueryPackageSideInput` (key: package hash)   |
| `PackageDependenciesAccessLocked`          | `QueryPackageDependencyCountSideInput`        |
| `PackageDependencyAliasAccessLocked`       | `QueryPackageDependencyAliasSideInput` (alias outcome is part of the key) |

Because the alias side-input encodes the *outcome* (`found` + `target_package_id`)
in its key, distinct results are tracked as independent edges — flipping
an alias from missing → present, or repointing it to another package,
correctly invalidates dependents.

## How a manifest entry becomes a `PackageInfo`

1. Driver reads the manifest JSON.
2. `RawPackageInfo::fromJson` produces a `RawPackageInfo` (validates the
   shape, reports diagnostics).
3. `createPackageInfo(raw, all_package, report)` loads the module tree under
   `package_path` and constructs a `PackageInfo`.
4. The `PackageInfo` is stored in global state and looked up by id from
   queries via `PackageAccessLocked`.