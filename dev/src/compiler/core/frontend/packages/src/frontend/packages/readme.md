# Frontend Packages

Resolved package metadata used by the rest of the frontend: package id,
version, feature flags and the list of direct dependencies (with their
aliases). Built from manifest input (`RawPackageInfo`) and stored in
global compiler state, then accessed from queries through `*AccessLocked`
wrappers that participate in the incremental query framework.

## Type layout

- **`RawPackageInfo` / `RawDependencyInfo`** — plain manifest data
  (`name`, `path`, `version`, `features`, `dependencies`). Produced by
  `fromJson`. Consumed by `createPackageInfo` to build a `PackageInfo`.
- **`PackageInfo`** — resolved package: keeps the root `ModuleID`,
  `version`, `features`, dependency list, and a stable hash derived from
  the package id. Lives in global state.
- **`PackageDependencyInfo`** — `(package_id, alias)` pair. The `alias`
  defaults to `package_id` when none is set in the manifest.

## Why locks (`*AccessLocked`)

The compiler is **query-based and incremental**. Anything a query reads
must be turned into a tracked dependency, so that when a value changes
the query is invalidated and re-run on the next build. For packages, the
inputs that can change between builds are:

- whether a given package exists at all (id + identity),
- the **size** of its dependency list (something added/removed),
- a specific `(package, alias) → target package id` lookup result
  (alias added, removed, or repointed).

> `version` and `features` are also tracked as query inputs — generated
> code can depend on them (feature-gated APIs, version conditionals), so
> any read must register the dependency or builds would silently go
> stale after a manifest edit. Every read goes through `*AccessLocked`,
> and unlocking registers the matching side-input query.

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

## When to `unlock(ctx)` vs. `illegalAccess()`

Rule of thumb:

- **Inside a query (you have a `query::Context&`) → always `unlock(ctx)`.**
  This registers the side-input dependency. Skipping it = silent stale
  results.
- **Outside the query system → `illegalAccess()`.**
  Examples: driver bootstrap, debug printing, REPL glue, anything that
  is not a query and never feeds a cached query result.

`PackageDependencyAliasAccessLocked::unlock` returns
`Optional<PackageAccessLocked>`: empty when the alias does not resolve.
The caller decides whether to unlock the inner `PackageAccessLocked`
further (only do so if the query actually depends on the resolved
package's identity).

## How a manifest entry becomes a `PackageInfo`

1. Driver reads the manifest JSON.
2. `RawPackageInfo::fromJson` produces a `RawPackageInfo` (validates the
   shape, reports diagnostics).
3. `createPackageInfo(raw, report)` loads the module tree under
   `package_path` and constructs a `PackageInfo`.
4. The `PackageInfo` is stored in global state and looked up by id from
   queries via `PackageAccessLocked`.
