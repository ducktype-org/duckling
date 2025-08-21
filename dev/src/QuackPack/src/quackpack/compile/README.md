### quackpack.compile

Temporary bridge to the Duckling compiler executable (`duckc`). Provides an interface that translates QuackPack storage and dependency info into compiler arguments and runs the compiler (or compile-and-run flow).

## `compiler.py`
`Compile` and `CompileAndRun` implementations; forms arguments and execs `duckc`.

## `code_sink.py`
Interfaces for connecting to a compilation backend and producing a continuation (e.g., exec/exit).
