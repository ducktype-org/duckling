# Coercions

This directory contains the information about how the Higher Type System (TSH) decides **coercions**, the implicit
conversions that happen when a value is handed to a location of a possibly different type (an
argument, a variable, a variant, a tuple element).

The README has two parts. [Part 1](#part-1-architecture) walks through the general architecture of this directory:
the two layers, the shape of their answers, and how ranks work.
[Part 2](#part-2-how-information-flows) follows the data flow through the two layers on a bigger example to explain
the logic of `ownership.cpp`.

All the trees below are real `debugPrint` output. To print one yourself, call `debugPrint` on a
`SymbolTypeCoercion` or a `Coercion`.

---

## Part 1: Architecture

### The question, split in two

To coerce a value into a location, the compiler has to answer two different kinds of questions:

1. **What happens to the type?** Is `ref i32 -> i64` possible, and how: deref first, then widen?
   Which alternative of `i32 | f64` does an `i16` go into? Can an immutable value go into a mutable
   slot? Which coercions create a need of materializing a value in a new place (hand it over)? 
   These questions depend only on the two `SymbolType`s.
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

```
CoercionTree<Source>
├─ source: Source                      what was asked about
└─ outcome: one of
   ├─ CoercionNode                     the coercion succeeded
   │  ├─ source, target: SymbolType
   │  ├─ rank: CoercionRank            the worst thing done in this node, parts included
   │  └─ steps: [CoercionStep]         in the order they happen
   │     ├─ kind: Deref | HandOver | ... | Elementwise { parts: [CoercionNode] }
   │     └─ result: SymbolType         the type of the value once the step is done
   └─ CoercionError                    the coercion was refused
      ├─ reason: IncompatibleTypes | SubPartRefused | ...
      ├─ source, target: SymbolType
      └─ causes: [Cause { at: CoercionPath, error: CoercionError }]
```

#### The coercion tree

A `CoercionNode` (`coercion_node.hpp`) describes **one value**. It holds the value's `source` and
`target` type and an ordered list of `CoercionStep`s. Each step has a *kind* (what is done) and a
`result` (the type of the value after the step), so the steps form a typed chain from `source` to
`target`. The step kinds are small tag structs collected in a `std::variant`. Every one of them
carries a `static constexpr` `RANK` and `NAME`, which is what the printer and the ranking read.
They are declared in the order of their ranks:

| Step               | Printed as              | Rank                  | Produced by                                                                        |
|--------------------|-------------------------|-----------------------|------------------------------------------------------------------------------------|
| `HandOver`         | `hand over`             | `Identity`            | `SymbolType` coercions only, `handOver()` in `rules.cpp`                                            |
| `ImplicitMove`     | `implicit move`         | `Identity`            | `ExpressionType` coercions only, replaces a `HandOver` in `ownership.cpp`                           |
| `MutabilityChange` | `mutability change`     | `MutabilityChange`    | `keepReference`, raw pointers, and a tuple whose parts only change mutability      |
| `Deref`            | `deref`                 | `Deref`               | `readThroughReference`                                                             |
| `Numeric`          | `numeric`               | `Numeric`             | `convertValue`: a wider integer or float, `bool` into an integer                   |
| `VariantPack`      | `pack as alternative N` | `VariantPack`         | `packIntoVariant`                                                                  |
| `LiftToType`       | `lift to type`          | `LiftToType`          | `convertValue`: `()` or a tuple of types into `type`                               |
| `UserConversion`   | `user conversion`       | `UserConversion`      | nothing yet (@TODO: #3656)                                                                |
| `ZeroCheck`        | `zero check`            | `ZeroCheck`           | `convertValue`: `byte`, `char` or a raw pointer into `bool`                        |
| `RetypeVoid`       | `retype void`           | `RetypeVoid`          | `retypeVoid`                                                                       |
| `Elementwise`      | `elementwise`           | the worst of its parts | `coerceElementwise`, and `ownership.cpp` for a value built out of parts           |

`HandOver` and `ImplicitMove` never meet in one tree. `CoercionTree::successfulCoercion` asserts
that a plan has no `ImplicitMove` and a coercion has no `HandOver`, in its parts too.

`Elementwise` is the one that makes the structure a tree. It takes a composite (a
tuple) apart and gives each component a child `CoercionNode` of its own, in `parts`. The position
of a child is simply its index in `parts`.

Here is a tuple literal `(makeBox(), &x, 1i32)`, where `x` is a local `i32`, coerced into
`(box i32, i64 | bool, i64)`. First the plan, which `QuerySymbolTypeCoercion` builds from the
types alone:

```
plan  Tuple(box i32, ref i32, i32)  ->  Tuple(box i32, Variant (bool, i64), i64)
  rank        VariantPack
  steps       elementwise  ->  Tuple(box i32, Variant (bool, i64), i64)
                component 0:  box i32  ->  box i32
                  rank        Identity
                  steps       hand over  ->  box i32
                component 1:  ref i32  ->  Variant (bool, i64)
                  rank        VariantPack
                  steps       deref  ->  i32
                              numeric  ->  i64
                              pack as alternative 1  ->  Variant (bool, i64)
                component 2:  i32  ->  i64
                  rank        Numeric
                  steps       numeric  ->  i64
```

Then the coercion, which `coercionOf` decides for this particular value:

```
coercion  Tuple(box i32, ref i32, i32) [Temporary, pure, allows MOVE|COPY|USE|DESTROY]  ->  Tuple(box i32, Variant (bool, i64), i64)
  rank        VariantPack
  steps       elementwise  ->  Tuple(box i32, Variant (bool, i64), i64)
                component 0:  box i32  ->  box i32
                  rank        Identity
                  steps       implicit move  ->  box i32
                component 1:  ref i32  ->  Variant (bool, i64)
                  rank        VariantPack
                  steps       deref  ->  i32
                              numeric  ->  i64
                              pack as alternative 1  ->  Variant (bool, i64)
                component 2:  i32  ->  i64
                  rank        Numeric
                  steps       numeric  ->  i64
```

How to read it:

- The first line says which layer answered (`plan` or `coercion`), the source and the target. A
  coercion also shows the `ValueCategory` of the root value in brackets.
- `rank` is the rank of the node, its parts included. `steps` lists the steps one per line, each
  with the type it produces. `nothing` means the node has no steps.
- The lines under an `elementwise` step are its parts, each with its own source, target, rank and
  steps.
- Variant alternatives are kept in a canonical order, so `i64 | bool` prints as
  `Variant (bool, i64)` and `i64` is alternative 1.
- The only difference between the two trees is component 0. The plan says the box is handed over
  there, and the coercion knows that `makeBox()` is a temporary, so the box is moved.
- There is no `hand over` for the `i32`s. `handOver()` skips a value that `isCopiedByBytes`
  (trivially copyable *and* trivially destructible, see `passing.hpp`), because there is nothing
  to decide for it.

#### The coercion error

A refusal has the same tree shape. A `CoercionError` (`coercion_error.hpp`) holds a `reason`, the
`source`/`target` at its level, and a list of `causes`:

| Reason                     | Printed as                       | When                                                                             |
|----------------------------|----------------------------------|----------------------------------------------------------------------------------|
| `IncompatibleTypes`        | `incompatible types`             | no rule fits, or no variant alternative fits                                     |
| `MutabilityMismatch`       | `mutability mismatch`            | an immutable value into a mutable location behind a `ref`/`box`                 |
| `SubPartRefused`           | `a part does not fit`            | this level would fit, but some of its components do not                          |
| `NotImplemented`           | `not implemented`                | meant to work one day, like function subtyping                                   |
| `AmbiguousCoercion`        | `ambiguous, alternatives 0, 1`   | several variant alternatives fit equally well                                    |
| `RequiresExplicitCopyMove` | `requires explicit copy or move` | layer two: the user has to write `copy` or `move`                                |
| `TypeNotCopyable`          | `type not copyable`              | layer two: the value would have to be copied, but its type cannot be             |

A cause is a child error together with a `CoercionPath` (`coercion_path.hpp`) that says where the
child sits: which component, which variant alternative, and so on. The path is stored on the edge
(`CoercionError::Cause`), not in the child, because the same child is often a cached plan
refusal that sits in many places at once. For example, the refusal `i32 -> f64` is a cause in
both components of `(i32, i32) -> (f64, f64)`. The root has no path, since it does not sit
anywhere. Only the parts that were refused are causes, and all of them are collected, not only
the first one.

When a parent was refused only because a child was, its reason is `SubPartRefused` and the real
explanation is further down. This makes it possible to say "the second element of the inner tuple
is an `f64`, not an `f32`" rather than just "types don't match". The `CoercionError` is also a
tree, but not only due to tuples. Packing a value into a variant may produce multiple coercion
errors, each explaining why the value does not fit into one alternative:

```
plan  Tuple(i32, Tuple(bool, f64))  ->  Variant (Tuple(i64, Tuple(bool, f32)), f64)
  refused     incompatible types
  caused by   alternative 0: a part does not fit: Tuple(i32, Tuple(bool, f64)) -> Tuple(i64, Tuple(bool, f32))
                component 1: a part does not fit: Tuple(bool, f64) -> Tuple(bool, f32)
                  component 1: incompatible types: f64 -> f32
              alternative 1: incompatible types: Tuple(i32, Tuple(bool, f64)) -> f64
```

Every line under `caused by` is one cause: its path, its reason, and the types at its level.
Indentation goes one level down per cause. An ambiguity has no causes of its own, it names the
alternatives that tied. Here it sits next to a plain mismatch in another component:

```
plan  Tuple(u8, f64)  ->  Tuple(Variant (u16, u32), f32)
  refused     a part does not fit
  caused by   component 0: ambiguous, alternatives 0, 1: u8 -> Variant (u16, u32)
              component 1: incompatible types: f64 -> f32
```

Layer two refuses with the same structure, for a value that the types allowed. In `(b, x)` into
`(box i32, i64)`, with locals `b: box i32` and `x: i32`, only the local box is the problem:

```
coercion  Tuple(box i32, i32) [Temporary, pure, allows MOVE|COPY|USE|DESTROY]  ->  Tuple(box i32, i64)
  refused     a part does not fit
  caused by   component 0: requires explicit copy or move: box i32 -> box i32
```

A `Deref` adds no level of its own to an error. `readThroughReference` takes the refusal of the
pointee and only replaces its `source` with the reference type:

```
plan  ref Tuple(i32, f64)  ->  Tuple(i64, f32)
  refused     a part does not fit
  caused by   component 1: incompatible types: f64 -> f32
```

### Ranks

A rank says how good of a match a coercion is. Ranks exist so that candidates can be compared,
like the alternatives of a variant or the overloads of a function.

`Rank` (`coercion_rank.hpp`) is an enum ordered from the best to the worst:

| `Rank`             | Meaning                                                                                 |
|--------------------|-----------------------------------------------------------------------------------------|
| `Identity`         | nothing happens to the value (`HandOver` and `ImplicitMove` have this rank)            |
| `MutabilityChange` | only the mutability the value is seen with changes                                      |
| `Deref`            | the pointee is read out of a `ref`/`box`                                                |
| `Numeric`          | a widening numeric conversion                                                           |
| `VariantPack`      | the value is packed into a variant                                                      |
| `LiftToType`       | the value is read as a type                                                             |
| `UserConversion`   | a user defined conversion (@TODO: #3656)                                                |
| `ZeroCheck`        | the value is compared against zero, which turns it into a `bool`                        |
| `RetypeVoid`       | a `void` value is seen as a value of another type                                       |

Every rank except `Identity` belongs to exactly one step of the same name.

A `CoercionRank` is the rank of **the worst thing a coercion does**. It has a single category,
there are no tie-breaking counters:

- `CoercionRank::combine` takes the worse of two ranks. `CoercionNode::append` combines the rank
  of the node with the rank of every step it appends, so no rule ever computes a rank by hand.
- An `Elementwise` step has the rank of its worst part, so a composite is as good as its worst
  component.

```
plan  ref i32  ->  i64
  rank        Numeric
  steps       deref  ->  i32
              numeric  ->  i64
```

Two ranks are compared with `dominates` (strictly better) and `ties` (equal). `bestCoercion`
(`best_coercion.hpp`) takes the `Candidate`s that did not refuse and returns:

- `Chosen` with the id of the candidate that is strictly better than every other one,
- `Tied` with the ids of all the best candidates, when there are several of them,
- `NoCandidate`, when the list is empty.

`packIntoVariant` uses it like this:

1. The source is planned into **every** alternative through the same query. An alternative that
   refuses becomes a cause with the path `alternative i`.
2. The others are compared by the rank of getting into the alternative. `NoCandidate` refuses with
   `IncompatibleTypes` and the collected causes. `Tied` refuses with `AmbiguousCoercion`.
3. The plan of the chosen alternative gets a `VariantPack` step appended. So a coercion that
   packs always ranks at least `VariantPack`, even when the alternative fits exactly:

```
plan  i32  ->  Variant (f64, i32)
  rank        VariantPack
  steps       pack as alternative 1  ->  Variant (f64, i32)
```

The pack itself is added after the choice, so it never decides between two alternatives. What
decides is how the value gets into an alternative, which may be a conversion:

```
plan  i32  ->  Variant (bool, i64)
  rank        VariantPack
  steps       numeric  ->  i64
              pack as alternative 1  ->  Variant (bool, i64)
```

An alternative that is a variant itself carries its own pack. For `i32 -> (i32 | bool) | i64`,
getting into the inner variant already ranks `VariantPack`, while getting into `i64` ranks
`Numeric`, so `i64` wins:

```
plan  i32  ->  Variant (Variant (bool, i32), i64)
  rank        VariantPack
  steps       numeric  ->  i64
              pack as alternative 1  ->  Variant (Variant (bool, i32), i64)
```

`u8 -> u16 | u32` ranks `Numeric` for both alternatives, so it is `ambiguous, alternatives 0, 1`.

Ranks come from plans only. Layer two never changes them, see
[Why does a decision never change the rank?](#why-does-a-decision-never-change-the-rank).

### Layer one: planning from types (`rules.cpp`)

`planCoercion` is a short ladder of cases, tried in order:

1. A `void` source is retyped into anything (`RetypeVoid`).
2. A `ref`/`box` target keeps the reference (`keepReference`). Which reference-kind transitions
   are legal is a small table in `reference_coercion.hpp`/`.cpp` (`referenceCoercionRule`).
   Behind a reference nothing converts, and only mutability may relax.
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

| `PassingMethod`      | when                                                                                                       | effect on the tree                  |
|----------------------|------------------------------------------------------------------------------------------------------------|-------------------------------------|
| `ByteCopy`           | trivially copyable, a literal, or already `move`d by the user                                              | the `HandOver` is dropped           |
| `ImplicitMove`       | an owned temporary that is not trivial                                                                     | replaced by `ImplicitMove`          |
| `ExplicitCopyOrMove` | a local, global or dereferenced value that is not trivially copyable, or a temporary that may not be moved out of (like a field of a temporary) | refused: `RequiresExplicitCopyMove` |
| `NotCopyable`        | a local, global or dereferenced value whose type cannot be copied at all                                   | refused: `TypeNotCopyable`          |

A value built out of parts, like a tuple literal, never gets to `passingMethod` as a whole. Its
`HandOver` is expanded into an `Elementwise`, and each part is handed over on its own, see
[Part 2](#part-2-how-information-flows). Every other step is copied unchanged.

`printing.cpp` holds the `debugPrint` implementations for all of the above. It prints a tree with
one step per line, labelled `plan` or `coercion` depending on which layer it came from. Use it
when a test fails.

---

## Part 2: How information flows

### A worked example

The cases below use the declarations of the test framework: # TODOP: Add coercion tester link

```
fun makeBox() -> box i32
fun makeBoxAndInt() -> (box i32, i32)

var x: i32 = 1;
var b: box i32 = new 1;
var t: (box i32, i32) = (new 1, 2);
```

We coerce one tuple literal whose four elements each show a different part of the logic:

```
value:  (makeBoxAndInt(), (move b, x), (makeBox(), 1i64), &x)
into:   ((box i32, i64), (box i32, i64 | bool), (box i32, i64), i64)
```

#### Step 1: the caller describes the value

The caller (HELIoS, or the test framework) turns the expression into a `ValueSource`. It
descends into tuple literals only, because only then more than one `ValueCategory` at once has to be represented. 
Every expression is a single leaf:

```
nested  ((box i32, i32), (box i32, i32), (box i32, i64), ref i32)  [Temporary]
├─ single  (box i32, i32)  [Temporary]                  makeBoxAndInt()
├─ nested  (box i32, i32)  [Temporary]                  (move b, x)
│  ├─ single  box i32  [Temporary, forces MOVE]         move b
│  └─ single  i32  [Local]                              x
├─ nested  (box i32, i64)  [Temporary]                  (makeBox(), 1i64)
│  ├─ single  box i32  [Temporary]                      makeBox()
│  └─ single  i64  [Literal]                            1i64
└─ single  ref i32  [Temporary]                         &x
```

#### Step 2: layer one plans the types

`coercionOf` asks `QuerySymbolTypeCoercion` for the coercion plan only based on the `SymbolType`s of the root types. In this example those would be `((box i32, i32), (box i32, i32), (box i32, i64), ref i32)` coerced into `((box i32, i64), (box i32, i64 | bool), (box i32, i64), i64)`. This would be the output tree of `QuerySymbolTypeCoercion`:
```
plan  Tuple(Tuple(box i32, i32), Tuple(box i32, i32), Tuple(box i32, i64), ref i32)  ->  Tuple(Tuple(box i32, i64), Tuple(box i32, Variant (bool, i64)), Tuple(box i32, i64), i64)
  rank        VariantPack
  steps       elementwise  ->  Tuple(Tuple(box i32, i64), Tuple(box i32, Variant (bool, i64)), Tuple(box i32, i64), i64)
                component 0:  Tuple(box i32, i32)  ->  Tuple(box i32, i64)
                  rank        Numeric
                  steps       elementwise  ->  Tuple(box i32, i64)
                                component 0:  box i32  ->  box i32
                                  rank        Identity
                                  steps       hand over  ->  box i32
                                component 1:  i32  ->  i64
                                  rank        Numeric
                                  steps       numeric  ->  i64
                component 1:  Tuple(box i32, i32)  ->  Tuple(box i32, Variant (bool, i64))
                  rank        VariantPack
                  steps       elementwise  ->  Tuple(box i32, Variant (bool, i64))
                                component 0:  box i32  ->  box i32
                                  rank        Identity
                                  steps       hand over  ->  box i32
                                component 1:  i32  ->  Variant (bool, i64)
                                  rank        VariantPack
                                  steps       numeric  ->  i64
                                              pack as alternative 1  ->  Variant (bool, i64)
                component 2:  Tuple(box i32, i64)  ->  Tuple(box i32, i64)
                  rank        Identity
                  steps       hand over  ->  Tuple(box i32, i64)
                component 3:  ref i32  ->  i64
                  rank        Numeric
                  steps       deref  ->  i32
                              numeric  ->  i64
```

- Components 0 and 1 change type, so `coerceElementwise` takes them apart, and every box inside
  gets its own `hand over`.
- Component 2 already has the target type, so `convertValue` hands the whole tuple over in one
  step. From the types alone that is right: an existing `(box i32, i64)` is copied or moved as a
  whole.
- Component 3 reads through the reference. There is no `hand over` after the `deref`, because an
  `i32` is copied by its bytes (is trivially copyable and has no destructor).
- Every one of these sub-plans was a query of its own, so the next coercion that needs, say,
  `ref i32 -> i64` takes it from the cache.
  
Now we know how to perform the coercion and where the value is passed to another location (it's handed over). We don't know HOW exactly the hand over should be performed. Thats what the second layer decides.

#### Step 3: layer two walks the plan with the value
Now for the more complex part. After `coercionOf` gets the `SymbolType` plan, it uses the plan together with `ValueSource` and enriches it with ownership information. This "essentially" means each `HandOver` node is replaced with the correct way to pass the value. The `HandOver` node can be replaced in four different ways:
- by nothing - when copying by bytes is enough (e.g. user wrote a `move`, for a
literal, an lvalue of a trivially copyable type and without a destructor)
- by an `ImplicitMove` - when the value is a temporary
- by an `ElementWise` - when the value is built out of parts (i.e. a tuple literal). Each part
is then handed over separately.
- by an error - when a user was supposed to write `copy` or `move` explicitly.

`coercionOf` walks the plan and calls `decideNode` on every node of the tree together with a `ValueSource` cursor, which says which
value the next step is about. It starts at the root with the whole literal:

| Plan node        | Cursor when the node starts                                    | What happens                                                                                                                                                                            |
|------------------|----------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| root             | the whole literal, `nested`                                    | `elementwise`: `decideParts` gives part `i` the cursor `partAt(i)`, which is the given part of the literal                                                                              |
| 0                | `makeBoxAndInt()`, a single `Temporary` tuple                  | `elementwise`: the value has no given sub-parts (so the `ExpressionType` of `makeBoxAndInt()` is the only one we care about. Inner values of the `(box i32, i32)` tuple have the same value category as `makeBoxAndInt()`). `partAt` derives them from the type. Each component inherits the category of the whole, so both are `Temporary`                         |
| 0.0              | `box i32 [Temporary]`, derived from (`makeBoxAndInt()`)                                 | `hand over`: `passingMethod` says `ImplicitMove`, since a temporary may be moved                                                                                                        |
| 0.1              | `i32 [Temporary]`, derived from `makeBoxAndInt()`                                     | `numeric` is copied as it is                                                                                                                                                            |
| 1                | `(move b, x)`, `nested`                                        | `elementwise`: `partAt` gives the two parts of the inner literal                                                                                                                        |
| 1.0              | `move b [Temporary, forces MOVE]`                              | `hand over`: the user already wrote `move`, so `passingMethod` says `ByteCopy` and the step is dropped                                                                                  |
| 1.1              | `x [Local]`                                                    | `numeric` and `pack as alternative 1` are copied. After each of them the cursor becomes `producedBy(step)`, a fresh temporary                                                           |
| 2                | `(makeBox(), 1i64)`, `nested`                                  | `hand over` of a value **built out of parts**: `decideHandOver` does not ask `passingMethod` about the whole tuple since it contains more than one `ExpressionType` inside. It plans each component into its own place (`planPartsInPlace`) and decides those plans against the parts. The `hand over` becomes an `elementwise` |
| 2.0 (new)        | `makeBox() [Temporary]`                                        | `hand over`: `ImplicitMove`                                                                                                                                                             |
| 2.1 (new)        | `1i64 [Literal]`                                               | the identity plan of an `i64` has no steps                                                                                                                                              |
| 3                | `&x [Temporary]`                                               | `deref`: the cursor becomes `dereferenced()`, an `i32 [Dereferenced]`. Then `numeric` is copied. Had there been a `hand over` after the `deref`, it would have been decided for the `Dereferenced` pointee |

The result:

```
coercion  Tuple(Tuple(box i32, i32), Tuple(box i32, i32), Tuple(box i32, i64), ref i32) [Temporary, pure, allows MOVE|COPY|USE|DESTROY]  ->  Tuple(Tuple(box i32, i64), Tuple(box i32, Variant (bool, i64)), Tuple(box i32, i64), i64)
  rank        VariantPack
  steps       elementwise  ->  Tuple(Tuple(box i32, i64), Tuple(box i32, Variant (bool, i64)), Tuple(box i32, i64), i64)
                component 0:  Tuple(box i32, i32)  ->  Tuple(box i32, i64)
                  rank        Numeric
                  steps       elementwise  ->  Tuple(box i32, i64)
                                component 0:  box i32  ->  box i32
                                  rank        Identity
                                  steps       implicit move  ->  box i32
                                component 1:  i32  ->  i64
                                  rank        Numeric
                                  steps       numeric  ->  i64
                component 1:  Tuple(box i32, i32)  ->  Tuple(box i32, Variant (bool, i64))
                  rank        VariantPack
                  steps       elementwise  ->  Tuple(box i32, Variant (bool, i64))
                                component 0:  box i32  ->  box i32
                                  rank        Identity
                                  steps       nothing
                                component 1:  i32  ->  Variant (bool, i64)
                                  rank        VariantPack
                                  steps       numeric  ->  i64
                                              pack as alternative 1  ->  Variant (bool, i64)
                component 2:  Tuple(box i32, i64)  ->  Tuple(box i32, i64)
                  rank        Identity
                  steps       elementwise  ->  Tuple(box i32, i64)
                                component 0:  box i32  ->  box i32
                                  rank        Identity
                                  steps       implicit move  ->  box i32
                                component 1:  i64  ->  i64
                                  rank        Identity
                                  steps       nothing
                component 3:  ref i32  ->  i64
                  rank        Numeric
                  steps       deref  ->  i32
                              numeric  ->  i64
```

Component 2 shows that the coercion tree **is not always the plan tree with steps replaced**.
Layer two can make it deeper. Handing `(makeBox(), 1i64)` over as one piece would be fine here,
but with a local box in it, like `(b, 1i64)`, the bytes of `b` would be copied and the box freed
twice. The rank stays `VariantPack`, the rank of the plan.

#### When the value is refused

Replace `makeBoxAndInt()` with the local `t` and `move b` with `b`. The types are the same, so the
plan is the same cached one, and only layer two answers differently:

```
coercion  Tuple(Tuple(box i32, i32), Tuple(box i32, i32), Tuple(box i32, i64), ref i32) [Temporary, pure, allows MOVE|COPY|USE|DESTROY]  ->  Tuple(Tuple(box i32, i64), Tuple(box i32, Variant (bool, i64)), Tuple(box i32, i64), i64)
  refused     a part does not fit
  caused by   component 0: a part does not fit: Tuple(box i32, i32) -> Tuple(box i32, i64)
                component 0: requires explicit copy or move: box i32 -> box i32
              component 1: a part does not fit: Tuple(box i32, i32) -> Tuple(box i32, Variant (bool, i64))
                component 0: requires explicit copy or move: box i32 -> box i32
```

- In component 0, `t` is a single `Local` value. `partAt` derives its box as a `Local` too, so the
  user has to write `copy` or `move`, although nothing in the plan is wrong.
- In component 1, the part `b` of the inner literal is a `Local` box.
- `decideParts` does not stop at the first refusal. It collects every refused part as a cause and
  refuses the node with `SubPartRefused`, which builds the same kind of tree as a plan refusal.
- Every refusal is about the *planned* node's types (`refusalOf`), so the levels match the plan:
  `Tuple(box i32, i32) -> Tuple(box i32, Variant (bool, i64))`, and not the types of the value.

## FAQ
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
  - after a `Deref`, the value is `Dereferenced` (`ValueSource::dereferenced`),
  - after any other step, the value is a new `Temporary` that belongs to the coercion
    (`producedBy`),
  - a component of an existing value inherits the category of the whole (`ValueSource::partAt`).

  Storing these categories in the nodes would add nothing. The one thing that *cannot* be derived
  is the category of the parts of a literal, and only `ValueSource` carries that.
- **After layer two, the category is already encoded in the step kinds.** "A `HandOver` was
  dropped", "it became `ImplicitMove`", "it became an `Elementwise` over the parts" and "it was
  refused" are the only outcomes of looking at a value. Once they are recorded, the consumer (HOUT
  creation) needs only the types of the steps to emit code.

The `Source` parameter is therefore mostly a **type-level tag** that tells "planned" apart from
"decided", plus the identity of the root value. It enables `implicitlyMoves()` only on `Coercion`,
and it switches the `plan`/`coercion` label in `debugPrint`.