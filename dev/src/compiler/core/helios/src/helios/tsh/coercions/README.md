# Coercions

This directory is a new way for the Higher Type System (TSH) to decide **coercions**, the implicit
conversions that happen when a value is handed to a location of a possibly different type (an
argument, a variable, a variant, a tuple element).

The README has two parts. [Part 1](#part-1-architecture) walks through the general architecture of this directory.
[Part 2](#part-2-how-information-flows) follows the data flow through the two layers of the solution to explain the complex logic.

---

## Part 1: Architecture

### The question, split in two

To coerce a value into a location, the compiler has to answer two different kinds of questions:

1. **What happens to the type?** Is `ref i32 → i64` possible, and how: deref first, then widen?
   Which alternative of `i32 | f64` does an `i16` go into? Can an immutable value go into a mutable
   slot? These questions depend only on the two `SymbolType`s.
2. **Who owns the value?** Is a `box i32` passed by moving it out of a temporary? Does the user
   have to write `copy`/`move`? Is the type not copyable at all? These questions depend on where
   the value came from, which is its `ValueCategory`. The type and the category together form an
   `ExpressionType`.

The design follows this split. `queries.hpp` declares both layers:

- `QuerySymbolTypeCoercion` (implemented in `rules.cpp`) answers question 1. It is a real query,
  cached on a `(source, target)` pair of `SymbolType`s. That pair is a good cache key: the same
  pairs come up over and over, and the rules call the query recursively for subparts, so every
  sub-plan is memoized too. Its answer is called a **plan** (`SymbolTypeCoercion`).
- `coercionOf` (implemented in `ownership.cpp`) answers question 2. It is a plain function. It
  takes the plan for the value's type and decides every ownership question in it for one concrete
  value, which gives a **coercion** (`Coercion`). Its input, a `ValueSource`, is too varied to be
  worth caching.

The plan leaves a marker wherever an ownership question has to be answered. That marker is the
`HandOver` step. The second layer is essentially "replace every `HandOver`, using what we know
about the value".

### The shape of an answer

Both layers answer with the same structure, `CoercionTree<Source>` (`coercion.hpp`). It is either
a successful tree rooted in a `CoercionNode`, or a `CoercionError`, and it also stores the
`Source` it was asked about. The two aliases are `SymbolTypeCoercion = CoercionTree<SymbolType<>>`
and `Coercion = CoercionTree<ExpressionType<>>`.

A `CoercionNode` (`coercion_node.hpp`) describes **one value**. It holds the value's `source` and
`target` type and an ordered list of `CoercionStep`s. Each step has a *kind* (what is done) and a
`result` (the type of the value after the step), so the steps form a typed chain from `source` to
`target`. The step kinds are small tag structs collected in a `std::variant`: `Deref`, `HandOver`,
`ImplicitMove`, `MutabilityChange`, `Numeric`, `ZeroCheck`, `LiftToType`, `RetypeVoid`,
`VariantPack`, `UserConversion`, and `Elementwise`. `Elementwise` is the one that makes the
structure a tree. It takes a composite (today only a tuple) apart and gives each component a child
`CoercionNode` of its own.

Every step kind also carries a `static constexpr` `RANK` and `NAME`. The ranks are the `Rank` enum
in `coercion_rank.hpp`, ordered from best (`Identity`) to worst (`RetypeVoid`). A `CoercionRank` is
the rank of the worst thing a coercion does: ranks combine by taking the maximum, and an
`Elementwise` step takes the worst rank among its parts. Ranks exist so that candidates can be
compared, which `bestCoercion` in `best_coercion.hpp` does by returning `Chosen`, `Tied` or
`NoCandidate`. Today only variant packing uses it. Overload resolution is meant to use it later.

A refusal has the same tree shape. A `CoercionError` (`coercion_error.hpp`) holds a `reason`
(`IncompatibleTypes`, `MutabilityMismatch`, `AmbiguousCoercion`, `RequiresExplicitCopyMove`, …),
the `source`/`target` at its level, and a list of `causes`. A cause is a child error together with
a `CoercionPath` (`coercion_path.hpp`) that says where the child sits: which component, which
variant alternative, and so on. When a parent was refused only because a child was, its reason is
`SubPartRefused` and the real explanation is further down. This makes it possible to say "the
second element of the inner tuple is a `string`, not an `i32`" rather than just "types don't
match". The `CoercionError` is also a tree, but this time not due to tuples, but due to variants.
Indeed, packing a value into a variant may produce multiple coercion errors, each explaining why
the value does not fit into one alternative.

### Layer one: planning from types (`rules.cpp`)

`planCoercion` is a short ladder of cases, tried in order:

1. A `void` source is retyped into anything (`RetypeVoid`).
2. A `ref`/`box` target keeps the reference (`keepReference`). Which reference-kind transitions
   are legal is a small table in `reference_coercion.hpp`/`.cpp` (`referenceCoercionRule`). The
   old coercion code shares this table. Behind a reference nothing converts, and only mutability
   may relax.
3. A variant target means packing into a variant (`packIntoVariant`). The rule plans the source
   into *every* alternative, ranks the plans that succeed with `bestCoercion`, and either extends
   the winning plan with a `VariantPack` step or refuses with `AmbiguousCoercion` or with the
   collected per-alternative refusals.
4. A `ref`/`box` source with a direct target is read through the reference
   (`readThroughReference`). The rule plans the pointee into the target and puts a `Deref` in
   front of the pointee's steps, in the same node.
5. Everything else is a plain value conversion (`convertValue`): numeric widening, zero checks,
   lifting to `type`, and tuples. A tuple is handled by `coerceElementwise`, which plans each
   component and builds an `Elementwise` step from the results.

Throughout the ladder, `handOver()` places a `HandOver` step wherever an existing value is taken
into the new location. It skips values that `isCopiedByBytes` (trivially copyable *and* trivially
destructible, see `passing.hpp`), because copying their bytes needs no ownership decision.

### Layer two: deciding ownership (`ownership.cpp`)

`coercionOf` takes the cached plan for `value.getSymbolType() → target`. A refused plan is
returned as it is, so type errors are never hidden behind ownership errors. A successful plan is
walked by `decideNode`. That walk never asks what a step *does*. It only keeps track of *which
value* the next step operates on, and keeps that value as a `ValueSource` (`value_source.hpp`).
At each `HandOver`, `passingMethod` (`passing.cpp`) looks at that value's category and gives one
of these answers:

| `PassingMethod`      | when                                                                 | effect on the tree                  |
|----------------------|----------------------------------------------------------------------|-------------------------------------|
| `ByteCopy`           | trivially copyable, a literal, or already `move`d by the user        | the `HandOver` is dropped           |
| `ImplicitMove`       | an owned temporary that is not trivial                               | replaced by `ImplicitMove`          |
| `ExplicitCopyOrMove` | a local, global or dereferenced value that is not trivially copyable | refused: `RequiresExplicitCopyMove` |
| `NotCopyable`        | same as above, but the type cannot be copied at all                  | refused: `TypeNotCopyable`          |

Every other step is copied unchanged.

`printing.cpp` holds the `debugPrint` implementations for all of the above. It prints a tree with
one step per line, labelled `plan` or `coercion` depending on which layer it came from. Use it
when a test fails.

---

## Part 2: How information flows

### A worked example

Take a local `x: i32` and a function `makeBox(): box i32`, and look at two tuple literals coerced
into `(box i32, i64)`.

**`(makeBox(), x)`**. The literal's type is `(box i32, i32)`, which differs from the target, so
layer one goes through `coerceElementwise`:

```
plan (box i32, i32) -> (box i32, i64)       |   coercion (box i32, i32)[temporary] -> (box i32, i64)
  Elementwise                               |      Elementwise
    [0] box i32 -> box i32:  HandOver       |        [0] ImplicitMove      <- part 0 is makeBox(), a temporary
    [1] i32 -> i64:          Numeric        |        [1] Numeric           <- no HandOver: i32 is copied by bytes
```

**`(makeBox(), 1i64)`**. The literal's type *is* `(box i32, i64)`, so the plan is just one
`HandOver` of the whole tuple. Handing the tuple over as one piece would be wrong if one of its
elements were a local `box`: its bytes would be copied and the box freed twice. Layer two
therefore uses the literal's parts and *expands* the `HandOver` into an `Elementwise` step:

```
plan (box i32, i64) -> (box i32, i64)       |   coercion ... -> (box i32, i64)
  HandOver                                  |      Elementwise
                                            |        [0] ImplicitMove
                                            |        [1] (nothing)
```

The second example shows that the coercion tree **is not always the plan tree with steps
replaced**. Layer two can make it deeper. The questions below explain how this works.

### Why do both trees hold only `SymbolType`s, even inside a `Coercion`?

`CoercionTree` is templated on `Source`, but `CoercionNode`, `CoercionStep::result` and
`CoercionError` are all written in terms of `SymbolType` only. The `ExpressionType` of a
`Coercion` appears **only once, at the root** (`getSource()`).

The short answer is that no consumer needs anything else. The longer answer has three parts:

- **The plan has to be value-free so it can be cached.** The nodes of a `Coercion` are copies of
  the plan's nodes, with only the `HandOver`s decided (`decideNode` starts each decided node from
  `planned_node.source/target`). Sub-plans are also copied straight out of the query cache
  (`parts.push_back(part->getRoot())`). Because of these copies, the node type must be the one the
  cache can store.
- **Every intermediate value category can be derived.** `decideNode` keeps the current category
  only while it walks, and three fixed rules produce it:
  - after a `Deref`, the value is `Dereferenced` (`ValueSource::dereferenced`);
  - after any other step, the value is a new `Temporary` that belongs to the coercion
    (`producedBy`);
  - a component of an existing value inherits the category of the whole (`ValueSource::partAt`).

  Storing these categories in the nodes would add nothing. The one thing that *cannot* be derived
  is the category of the parts of a literal, and only `ValueSource` carries that.
- **After layer two, the category is already encoded in the step kinds.** "A `HandOver` was
  dropped", "it became `ImplicitMove`", and "it was refused" are the only three outcomes of looking
  at a category. Once they are recorded, the consumer (HOUT creation, later) needs only the types
  of the steps to emit code.

The `Source` parameter is therefore mostly a **type-level tag** that tells "planned" apart from
"decided", plus the identity of the root value. It enables `implicitlyMoves()` only on `Coercion`,
and it switches the `plan`/`coercion` label in `debugPrint`.

### What is `ValueSource`, really?

A `ValueSource` is **a partial, lazily extended tree of `ExpressionType`s**. Its shape matches the
plan's `source` types, but it is only spelled out as deep as the *syntax* gives information that
the types cannot:

- for an ordinary expression, it is one `ExpressionType` and no parts (`singleValueSource`);
- for a tuple literal, it is the literal's `ExpressionType` plus one `ValueSource` per element
  (`nestedValueSource`), built recursively by the caller (see `describeValue` in
  `tests/coercion_tester.hpp`).

The reason for the extra tree: a tuple literal's own `ExpressionType` says "temporary", but its
elements may be a local `box`, a `move b`, and a function result, and each needs a different
answer. When there are no explicit parts, the missing levels are produced on demand as described
above. `partAt` returns the given part if there is one and otherwise derives it from the parent,
`dereferenced()` returns a fresh `Dereferenced` value, and so on. During the walk, `decideNode`
uses the `ValueSource` as a cursor that moves in step with the plan:

| plan step     | what happens to the cursor                                   | what happens to the tree |
|---------------|--------------------------------------------------------------|--------------------------|
| `Deref`       | becomes `dereferenced()`, a single `Dereferenced` value      | step copied              |
| `Elementwise` | split with `partAt(i)`, one sub-cursor per child node        | children decided recursively |
| `HandOver`, single value | its category is checked with `passingMethod`, then the cursor becomes `producedBy(step)` | step dropped, replaced by `ImplicitMove`, or refused |
| `HandOver`, value built out of parts | each part is handed over *into its own slot* | **replaced by a new `Elementwise`** whose children are the identity plans `slot → slot` (`planPartsInPlace`), decided against the parts |
| anything else | becomes `producedBy(step)`, a fresh temporary                 | step copied              |

Some consequences:

- `isBuiltOutOfParts()` means "the syntax supplied the parts", not "the type has components". A
  non-literal tuple has components but no parts.
- `dereferenced()` asserts that the value is not built out of parts, because a literal is never
  behind a reference. `partAt()` asserts that each given part has exactly the component type the
  plan expects, so the `ValueSource` and the plan cannot drift apart.
- A value that the user already wrote `move` on is a `ByteCopy`. The move is in the user's code, so
  the coercion does not add another one.

### Why does a decision never change the rank?

`HandOver` and `ImplicitMove` both have rank `Identity`, and the `Elementwise` that replaces a
`HandOver` contains only identity plans. Deciding ownership therefore never changes a coercion's
rank: the rank of a `Coercion` is the rank of its plan. This is what lets candidate ranking work
on cached plans alone (variant packing already does). Overload resolution should be able to do
the same, and only call `coercionOf` for the winner. If you add a step that layer two can
introduce, keep this property or rethink the ranking.

### Invariants that are conventions, not types

Some rules are enforced only by how the code builds trees, so keep them in mind when you edit
`rules.cpp` or `ownership.cpp`:

- **`HandOver` appears only in plans and `ImplicitMove` only in coercions.** Both are kinds of the
  same `CoercionStep` variant. The type system does not stop you from mixing them.
- **`Deref` is always the first step of a node.** `readThroughReference` merges the pointee's steps
  into the deref'ing node instead of nesting a child, and `CoercionNode::derefs()` and
  `referenceCoercion()` only look at `steps.front()`. Variant packing runs before reading through a
  reference, so a `ref T → U | V` plan is "the alternative's plan (starting with `Deref`) +
  `VariantPack`", which keeps the rule.
- **`HandOver` comes right at the start of a node, or right after its `Deref`.** The value is taken
  first and converted afterwards, so the value whose category is checked is the original one (or
  the pointee). For example, `ref (box i32, i32) → (box i32, i32)` is refused because the
  `HandOver` sees a `Dereferenced` tuple that holds a `box`.
- **A refused plan short-circuits.** `coercionOf` returns the plan's own error, so a `Coercion` can
  hold a `CoercionError` with type-only causes. Ownership refusals are built by `refusalOf` from
  the *planned* node's types. For a literal, that means the error is about the whole tuple, with
  the culprit element listed as a `Component` cause.
- **Success and failure are `std::variant`s.** This applies to `CoercionTree::Storage`, and to
  `DecidedNode`/`DecidedParts` in `ownership.cpp`. These are hand-rolled `std::expected`s that
  build a refusal tree from sub-refusals instead of stopping at the first error.

### Not wired up yet

Several pieces are declared and not used yet. `UserConversion` and `Rank::UserConversion` wait
for #3656. `ReferenceCoercion::points_to_source` and the `MutabilityMismatch` semantics wait for
#1488. Function-to-function coercion stops at `NotImplemented`, so `CoercionPath::Parameter` and
`Result` are never produced. `ValueSource` knows only tuple literals; array literals are #1920.
