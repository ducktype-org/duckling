Module with a class managing the debug info for various code representations,
whose most important user is Duck VM.
It should be serializable and parsable.

`DebugInfo::debugPrint(os)` (and `DebugInfo::toString()`, for a debugger) writes what a
`DebugInfo` holds as an indented, human-readable block: target, module path, position kind,
then every function with its parameters, instruction positions and variable initializations,
and finally the types; `SourcePosition::toString()` does the same for a single position. The
format is not stable - nothing should parse it, use `debug_info_io.hpp`
for that.
