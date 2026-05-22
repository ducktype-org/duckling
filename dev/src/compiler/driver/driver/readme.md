# Driver

The Driver module is responsible for encapsulating high-level compiler operations and initializations into simple drivers or functions, that call the core compiler components underneath.
Driver module for example automates process of initializing query framework state, transforming HUs into binary outputs, compilation of a package and all high-level operations of incremental compilation.

## REPL and Script Utilities

Driver helpers used by REPL and script execution are documented in:

* [driver/repl_utils](./src/driver/repl_utils/readme.md)
