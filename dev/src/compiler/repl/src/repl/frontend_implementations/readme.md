# REPL Frontend Implementations

Terminal UI backends that implement the [ReplFrontend](../frontend.hpp) interface.

## Implementations

Main one is `replxx` with many nice features(like syntax highlighting, completions, ...)  built on the [replxx](https://github.com/AmokHuginnsson/replxx) library.
There is also `minimal` implementation which acts as a fallback solution without any external dependencies.
The used implementation can be selected at the compile time with:
- `cmake -DUSE_REPLXX=ON ..`  - to use `replxx`
- `cmake -DUSE_REPLXX=OFF ..`  - to use `minimal`

## How the Implementations Work


### `minimal` implementation

- Implements its own line editor using direct terminal control (via POSIX or Windows APIs):
	- Sets raw mode to disable line buffering and echo.
	- Reads input one character at a time and processes special keys (arrows, backspace, etc.).
	- Manages cursor movement and screen updates using ANSI escape sequences.
- Maintains its own input history and supports multiline editing.
- Does not provide syntax highlighting or tab completion.


### `replxx` implementation


- The `FrontendReplxxImplementation` class wraps and extends the replxx library:
  - Syntax highlighting is implemented by setting a highlighter callback on the replxx instance, which colors tokens based on Duckling keywords, types, literals, and comments.
  - Tab completion and input hints are provided by registering completion and hint callbacks, which use sets of keywords, types, and user-collected identifiers from previous input.
  - Persistent history is managed by explicitly loading and saving to a file (`~/.duckling_repl_history`) in the constructor and destructor.
- Most input handling, navigation, and rendering is performed by replxx, with language-specific logic layered on top via these callbacks and hooks.


## Related Documentation

- [REPL Module](../../readme.md)

---