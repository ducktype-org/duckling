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

### General case
Assume that we have a package `P`, identified by its identity `I(P)` and version `V(P)`.
Assume that it has a dependency on a package `Q` and the dependency is described by an identity `I(Q)` and a list of possible versions `V(Q)_1, ..., V(Q)_n` (those are all the versions compatible with the ones described in the manifest of `P`).

Then we would have the following inequalities:

1. Forcing the presence of `Q` in one of the good versions (`var(P)` implies any of `var(P->Q_1_)`, ..., `var(P->Q_n_x)`), where `var(P->Q_i_x)` is a variable signifying that `Q` in version `V(Q)_i` was chosen to satisfy the dependency.

Note: if the dependency is only forced by some features of `P`, instead of `var(P)` we use `var(P, F)`, for any such feature `F`.

```
var(P) - var(P->Q_V1_x) - ... - var(P->Q_Vn_x) <= 0
```

2. Forcing the presence of `Q` with the appropriate features.
`P` forces `Q` to be present with some features by default, moreover features of `P` can force additional features of `Q`.

Assume that a feature `F_P` of `P` forces features `G_1, ..., G_n` of `Q`.
Then we get the following inequality:

```
n * var(P, F_P) - var(P->Q_x_G1) - ... - var(P->Q_x_Gn) <= 0
```

3. Forcing that if we choose a version realisation (pt 1), then the package is present.

Well, if we choose to realise the dependency by `V(Q)_i`, then `Q` in version `V(Q)_i` has to be present, so we get the following inequality (for all `i`):

```
var(P->Q_i_x) - var(Qi) <= 0
```

4. Forcing that if we choose a feature realisation (pt 2), then the chosen version realisation has that feature,
so for example, for all the versions `V_i` and all the features `G_j` we get the inequality:

```
var(P->Q_i_x) + var(P->Q_x_Gj) - var(QiGj) <= 1
```

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
The [``solver engine``](solver_engine.rs) yields which new packages (outside of `Prev`) have to be added, with what features (and what features to add to `Prev`) and what new dependencies have been realised and how.
It is the case that `Prev` union'd with this output is a correct resolution, but it may be possible to trim it, since we have accounted for the added/changed dependencies, but haven't removed dependencies removed from the manifest since the last compilation.

So for example, if our project depended on `a` and `b`, and then we have removed the dependency on `b` and added the dependency on `c`, `a` and `b` belong to `Prev` and [``solver engine``](solver_engine.rs) finds a solution to the dependency on `c`, reusing as many packages added for `a` and `b` as needed. But now, the packages used only to satisfy the dependencies of `b` can be simply removed, and this is done in the `new_freeze_generation` module.

Note that we create variables for preexisting packages (just in case) but do not care whether they are set to `1` or `0`.
