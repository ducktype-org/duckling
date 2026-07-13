# Formatter

Token-based source formatter for Duckling, plus `duckfmt`, the command-line tool
built on top of it. It re-renders a Duckling source file with normalized
whitespace and indentation while preserving the meaning of the code.

> **Note:** This module (the `Formatter` library, the `duckfmt` CLI, their tests,
> and this README) was written by AI.

## What it is

The formatter does **not** parse Duckling into an AST. It works one level lower,
on the **token stream** produced by the lexer (`tokenizer::TokenSource`,
tokenized with `keep_comments = true` so comments survive). Working on tokens
rather than a syntax tree keeps the formatter simple, fast (no query state, no
driver), and robust against incomplete or in-progress code — but it also bounds
what the formatter can know. See *Limitations* below.

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
│   ├── formatter.hpp / .cpp    # public API: formatTokens(...) wiring the pipeline
│   ├── token_classes.hpp/.cpp  # token predicates (line comment? code block? breakable group?)
│   ├── spacing.hpp / .cpp      # needSpace: the inter-token spacing oracle
│   ├── source_text.hpp / .cpp  # token text reproduction + source line queries
│   ├── statement_tree.hpp/.cpp # phase A: tokens -> statements
│   ├── doc.hpp / .cpp          # the layout document (Doc IR) node types
│   ├── doc_builder.hpp / .cpp  # phase B: statements -> Doc
│   └── doc_renderer.hpp / .cpp # phase C: Doc -> formatted text
├── tests/
│   ├── formatter_test.cpp      # unit tests (check input -> golden output)
│   └── snippets/*.duck         # sample sources used by the tests
└── format_main/
    ├── CMakeLists.txt          # the duckfmt executable
    └── src/main.cpp            # CLI entry point
```

The module depends only on the lexer (`make_module(Formatter USES Lexer ...)`),
not on the compiler frontend, MIR/LIR, or backends, so it lives as its own
top-level module beside `src/base`, `src/common`, and `src/compiler`.

## Public API

```cpp
#include <formatter/config.hpp>
#include <formatter/formatter.hpp>

std::string formatter::formatTokens(
    const lexer::TokenData& tokens,   // result of tokenize(keep_comments=true)
    const FormatConfig&     config);
```

The returned string is the formatted source, terminated by a single trailing
newline. Tokens are self-describing: verbatim text (format strings) and source
layout (blank lines, trailing comments) are recovered through the token
positions, which reference the `TokenSource` the tokens came from.

## How it works

`formatTokens` runs a three-phase pipeline:

1. **Structure** (`statement_tree`) — splits the token stream into statements.
   A statement ends at a `;`, at a line comment, at a `case` keyword (match
   arms), or after a `{...}` code block — unless `else` follows, so an
   `if/else if/else` chain stays one statement. A curly group is a *code block*
   (as opposed to an inline literal like `{1, 2, 3}`) when it contains a `;`, a
   nested curly block, a comment, a `case` arm, or a statement-opening keyword.

2. **Build** (`doc_builder`) — translates statements into a **layout document**
   (`doc.hpp`), a tree of text with the line-breaking decisions left open:
   - all spacing is decided here through `needSpace` (`spacing.hpp`), the oracle
     answering whether two adjacent tokens need a space (member access binds
     tightly, adjacent operators must stay separated so the lexer cannot merge
     them back differently, and so on);
   - every bracket group becomes a **Group** node, which renders flat when it
     fits and otherwise *explodes* one comma-separated element per line;
   - over-long expressions become **Fill** nodes that break greedily, preferring
     method-chain dots (`foo(a).bar` breaks before `.bar`) and falling back to
     spaced binary operators;
   - line comments attach to the statement or group element they trail in the
     source, and force every enclosing group to explode (a line comment consumes
     the rest of its line, so it can never render mid-group).

3. **Render** (`doc_renderer`) — walks the document once, tracking the current
   column, and makes every breaking decision against `max_line_length`. Every
   node caches the width of its single-line rendering, so each fits-check is
   O(1). Columns are measured **visually**: a tab counts as `indent_width`
   columns, so wrapping matches what an editor shows. Over-long line comments
   re-flow at word boundaries here, repeating the full `#`/`##` prefix on each
   continuation line.

## Configuration

`FormatConfig` (see `config.hpp`) controls the output. Defaults match the
prevailing Duckling style.

| Option                  | Default | Meaning                                              |
| ----------------------- | ------- | ---------------------------------------------------- |
| `indent_style`          | `Tab`   | indent with tabs or spaces                           |
| `indent_width`          | `4`     | visual columns per indent level                      |
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

# run the duckfmt CLI integration tests
python3 toolbox.py itest -b build -t "integration_tests/formatter"
```

The unit tests are golden tests: each feeds an input string (or a
`tests/snippets/*.duck` file) through `formatTokens` and compares against the
expected output, also asserting idempotence (formatting the output again is a
no-op) and round-trip safety (the significant token stream is unchanged).

## Limitations

Because the formatter sees only tokens, some constructs are ambiguous and are
handled conservatively to stay round-trip safe. For example, `while (a) {b}`
keeps `{b}` inline: a one-expression block with no keyword or `;` is
indistinguishable, at the token level, from a single-element set literal. When in
doubt the formatter changes nothing rather than risk altering meaning.
