Prerequisites
-------------

From the execution of the earlier phases of quackpack and the solver module we assume that we have the following information:
1. Packages already present in the previous freezefile and with what features they were present.
This set (let us call it `Prev`) should be trimmed of any packages which dependencies fail to resolve in `Prev`.

The assumption is that `Prev` already resolves most of the dependencies of the main package and the actual work to perform is minimal.

2. Packages which may be used to solve the yet-unresolved dependencies of the main package, with their manifests and possibly required features.
I assumed this also contains `Prev`, because to verify that `Prev` is dependency closed we need manifests of its members.

3. Which dependencies of the main package are unresolved in `Prev`.

Linear program
--------------

### Introduction

We translate the dependency solving problem into boolean conditions (conjuction implies conjunction, conjunction implies disjunction etc.) of variables, symbolizing presence of package/feature.
Then we rewrite them as an Integer Linear Program with boolean variables.

### General case
Assume that we have a package `P`, identified by its identity `I(P)` and version `V(P)`.
Assume that it has a dependency on a package `Q` and the dependency is described by an identity `I(Q)` and a list of possible versions `v_1, ..., v_n` (those are all the versions compatible with the ones described in the manifest of `P`).

Then we would have the following inequalities:

1. Forcing the presence of `Q` in one of the good versions (`var(P)` implies any of `var(Q_1)`, ..., `var(Q_n)`), where `Q_i` is `Q` in version `v_i`.

Note: if the dependency is only forced by some features of `P`, instead of `var(P)` we use `var(P, F)`, for any such feature `F`.

```
var(P) - var(Q_1) - ... - var(Q_n) <= 0
```

2. Forcing the realization to have required features.
We look at the pairs `F_1, F_2`, where `F_1, F_2` are features of `P`/empty.
If `F_1` enables the dependency, we look at the features `G_1, ..., G_n` forced by `F_2` on `Q`.

Then we add a condition that if `(P, F_1)`, `(P, F_2)` and `Q_i` are present, then `(Q_i, G_1), ..., (Q_i, G_n)`
are present as well.

```
n * (var(P, F_1) + var(P, F_2) + var(Q_i)) - var(Q_1, G_1) - ... - var(Q_i, G_n) <= 2 * n
```

Note: if `Q_i` does not support one of `G_1, ..., G_n`, then we forbid a triple `(P, F_1)`, `(P, F_2)`, `Q_i`
from occuring.

```
var(P, F_1) + var(P, F_2) + var(Q_1) <= 2
```

Important: We only need two types of conditions and no intermediate variables,
because of the assumption that each package can occur in just one version.
If this condition is lifted, then the program above becomes overly restrictive,
as for example both `Q_i`, `Q_j` could be present with only `Q_i` might realize the dependency,
but we will force the same features on `Q_j`.

### Preexisting parent and child
If the dependency was resolved in the previous freeze, we try to utilise this fact (if not, there would be no gain from making solver incremental).

Let us say that in the previous freeze the chosen realisation was `Q` in version `V(Q)`.
Then the only thing that can happen is that in the new freeze `P` may appear with new features which can force some additional features of `Q`.

For each possible feature `F_i` of `P`, if it was not in the previous freeze and it forces some features `G_1, ..., G_n` of `Q`, not present in the previous freeze, there are two cases.

1. Some `G_j` is not a valid feature of `Q`.
In such case we have to forbid `P` with `F_i`.
We could try to choose another realization but then we would have two versions of the realization
(for simplicity we assume that all preexisting packages are still present), which is forbidden.
2. Otherwise we simply add a condition that `P` with `F_i` forces `G_1, ..., G_n` on `Q`:
```
n * var(P, F_i) - var(Q, G_1) - ... - var(Q, G_n) <= 0
```

### Features expansion
For every package `P` and every its feature `F` such that either `P` or `F` is not present in the previous freeze,
if `F` expands to some features `F_1`, ..., `F_n != F` we add that `F` implies `F_1`, ..., `F_n` to the linear program.

### At most one version
To circumvent weird errors, QuackPack disallows packages with same identities (but different versions) from occuring in one build graph.

If, for a given identity, we have a preexisting package, all other versions are prohibited.
Otherwise we add a constraint that at most one version is present.

Outcome
-------
The new freeze, which consists of previous-freeze packages and features, as well as the packages and features chosen by the solution to the ILP.

Note that we create variables for preexisting packages (just in case) but do not care whether they are set to `1` or `0`,
we always put them into the new freeze.
