# Formatter

Token-based source formatter for Duckling, plus `duckfmt`, the command-line tool
built on top of it. It re-renders a Duckling source file with normalized
whitespace and indentation while preserving the meaning of the code.

> **Note:** This module (the `Formatter` library, the `duckfmt` CLI, their tests,
> and this README) was written by AI.

## What it is

The formatter does **not** parse Duckling into an AST. It works one level lower,
on the **token stream** produced by the lexer (`tokenizer::TokenSource`,
tokenized with `keep_comments = true` so comments survive). It walks that stream
and emits text, deciding for each pair of adjacent tokens whether they need a
space, a newline, or indentation.

Working on tokens rather than a syntax tree keeps the formatter simple, fast (no
query state, no driver), and robust against incomplete or in-progress code — but
it also bounds what the formatter can know. See *Limitations* below.

### Round-trip safety

Because the whole token stream (comments included) is preserved, the result is
**round-trip safe**: re-tokenizing the formatted output yields the same sequence
of significant tokens as the input. The single deliberate exception is an
over-long line comment, which is re-flowed onto several `#` lines — the comment
text is preserved, but one comment token becomes several.

## Layout

```
src/formatter/
├── CMakeLists.txt              # Formatter library + test + add_subdirectory(format_main)
├── README.md                   # this file
├── src/formatter/
│   ├── config.hpp / .cpp       # FormatConfig: options + JSON parsing
│   ├── formatter.hpp           # public API: formatTokens(...)
│   └── formatter.cpp           # the Emitter (all formatting logic)
├── tests/
│   ├── formatter_test.cpp      # unit tests (check input -> golden output)
│   └── snippets/*.duck         # sample sources used by the tests
└── format_main/
    ├── CMakeLists.txt          # the duckfmt executable
    └── src/main.cpp            # CLI entry point
```

The module used to live under `src/compiler/`. It depends only on the lexer
(`make_module(Formatter USES Lexer ...)`), not on the compiler frontend, MIR/LIR,
or backends, so it now lives as its own top-level module beside `src/base`,
`src/common`, and `src/compiler`.

## Public API

```cpp
#include <formatter/config.hpp>
#include <formatter/formatter.hpp>

std::string formatter::formatTokens(
    const lexer::TokenData& tokens,   // result of tokenize(keep_comments=true)
    const FormatConfig&     config,
    std::string_view        source);  // original text, to reproduce e.g. format strings
```

The returned string is the formatted source, terminated by a single trailing
newline.

## How it works

`formatTokens` builds an `Emitter` and runs it over the top-level token list.
The Emitter is a recursive walker; the main pieces are:

- **`needSpace(prev, cur)`** — the spacing oracle. Given two adjacent tokens it
  returns whether a single space goes between them. This is where rules like
  "no space before `;`/`,`", "member access (`.`, `.*`, `.?`) binds tightly",
  "type keyword hugs its array brackets (`i32[5]`)", and "two operator tokens
  must stay separated so they don't re-merge into a different operator" live.
- **`emitStatement` / `emitBlockBody`** — split a token run into statements and
  emit them one per line with the right indentation, preserving (capped) blank
  lines between them.
- **`isBlockCurly`** — decides whether a `{ ... }` group is a *code block*
  (exploded onto multiple indented lines) or an *inline literal* like
  `{1, 2, 3}`. A curly is a block when it contains a `;`, a nested curly block,
  a comment, a `case` arm, or a statement-opening keyword (`return`, `while`, …).
- **Wrapping** — when an inline construct would exceed `max_line_length`, the
  Emitter wraps, in this priority order:
  1. bracket groups with top-level commas **explode** one element per line;
  2. expressions **break at method-chain dots** (`chainBreakPoints`);
  3. expressions **break around binary operators** (`operatorBreakPoints`);
  4. long line comments **re-flow** onto continuation `#` lines.
  Unbreakable content (e.g. one long literal) may still exceed the limit.

## Configuration

`FormatConfig` (see `config.hpp`) controls the output. Defaults match the
prevailing Duckling style.

| Option                  | Default | Meaning                                              |
| ----------------------- | ------- | ---------------------------------------------------- |
| `indent_style`          | `Tab`   | indent with tabs or spaces                           |
| `indent_width`          | `4`     | spaces per level (only when `indent_style == Space`) |
| `max_line_length`       | `100`   | soft target; breakable constructs wrap past it       |
| `max_empty_lines`       | `2`     | max consecutive blank lines kept between statements  |
| `space_around_operators`| `true`  | `a + b` vs `a+b`                                      |

`FormatConfig::fromJson` reads the same options from JSON (keys `indentStyle`,
`indentWidth`, `maxLineLength`, `maxEmptyLines`, `spaceAroundOperators`);
unknown keys are ignored, missing keys keep their default.

## The `duckfmt` CLI

```
duckfmt <file> [options]
  -c, --config <path>   JSON formatter config file
  -i, --in-place        write the result back to the file instead of stdout
  -k, --check           print nothing; exit non-zero if the file is not formatted
```

By default it prints the formatted file to stdout. `--check` is the CI-friendly
mode (exit code reports whether the file is already formatted). Unlike `duckc`,
`duckfmt` does not initialize the compiler driver — it only tokenizes and
formats.

## Building and testing

```sh
# build the duckfmt binary
cmake --build build --target duckfmt -j"$(nproc)"

# run the formatter unit tests (pack: formatter)
python3 toolbox.py test -b build -R formatter_test -j"$(nproc)" --output-on-failure
```

The unit tests are golden tests: each feeds an input string (or a
`tests/snippets/*.duck` file) through `formatTokens` and compares against the
expected output, also asserting idempotence (formatting the output again is a
no-op).

## Limitations

Because the formatter sees only tokens, some constructs are ambiguous and are
handled conservatively to stay round-trip safe. For example, `while (a) {b}`
keeps `{b}` inline: a one-expression block with no keyword or `;` is
indistinguishable, at the token level, from a single-element set literal. When in
doubt the formatter changes nothing rather than risk altering meaning.
