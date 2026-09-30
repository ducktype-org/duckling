### mangling scheme

This is the most up-to-date version of the mangling scheme. If any changes are made,
either in the scheme or it's implementation, they should be reflected here.

```rust

<mangled-symbol-name> ::= <language-prefix> <scheme-version> <encoding> <opt-metadata>
                        | <no-mangling>                     // C linkage (builtins, extern C, special e.g. main)

// <no-mangling> is the plain declared name, except for an extern("C") symbol carrying
// `@c_symbol_name("<name>")`, which links under <name> instead.

// note: global identifiers starting with underscore and a capital letter are reserved in C
// Q seems to be free and stands for both query and quack
<language-prefix> ::= "_Q"

<scheme-version> ::= <compact-number>                       // version of the mangling scheme

<encoding> ::= <path>                                       // variables and constants
             | <path> <function>                            // functions
             | <repl-input-wrapper>                         // REPL/script input statements

// Wrappers of REPL/script input statements (expressions, instructions and global variable
// initializers) use simplified mangling for now. @TODO: #1768 decide if it's correct.
<repl-input-wrapper> ::= "__repl_input_wrapper_" <base-10-number>

<path> ::= <path-prefix> <symbol-name>
         | <back-reference>

<path-prefix> ::= "P" <package-name> <module-name>+                 // module-path of the module in a package
                | "S" <script-name>                                 // standalone script
                | "M" <module-name>                                 // standalone module
                | "R" <package-name> <module-name>+ <script-name>   // script in a package
                | <back-reference>
<package-name> ::= <identifier>                                     // package name
<module-name> ::= <identifier>                                      // module name
                | <identifier> "I" <templ-arg>* "E"                 // templated module instantiation
<script-name> ::= <identifier>                                      // script name

// note that function symbols nad function types are different
<function> ::= <function-type> <parameter-name>* "E"
               where <fumction-type>.<argument-type>.count == <parameter-name>.count

<parameter-name> ::= <identifier>

// base-10-number - the length in bytes of the raw identifier
// raw-identifier - the actual name of the symbol encoded according to the encoding-identifier
<identifier> ::= <encoding-identifier> <base-10-number> <raw-identifier>
               | <back-reference>
// if the identifier contains a unicode character, it is encoded using punycode
<encoding-identifier> ::= ""                                // no encoding
                        | "U"                               // punycode

<symbol-name> ::= "N" <name-prefix>+ <unscoped-name> "E"    // nested symbol
                | "G" <unscoped-name>                       // global symbol
                | "S" <unscoped-name>                       // standard library symbol in std.*
                | "M" <name-prefix>+ <unscoped-name> "E"    // standard library symbol in nested namespace (other letters may be used later to shorten those)

// encodes the enclosing scope
<name-prefix> ::= <identifier>                              // namespace or class name
                | <identifier> "I" <generic-arg>* "E"       // instantiation of a generic namespace, class, or function
                | <identifier> "I" <templ-arg>* "E"         // instantiation of a template namespace, class, or function
                | <back-reference>

<generic-arg> ::= <type>

<templ-arg> ::= <type>
              | <variadic-arg>
              | <value-arg>

<type> ::= <builtin-type>
         | <pointer-type>
         | <variant-type>
         | <tuple-type>
         | <array-type>
         | <class-type>
         | <function-type>
         | <unnamed-type-name>
         | <type-modifier>* <type>
         | <back-reference>
// there could possibly be more qualifiers
// should not repeat
<type-modifier> ::= "N"                                     // const
                  | "M"                                     // unique
                  | "L"                                     // leaking
                  | "R"                                     // reference
                  | "X"                                     // box

// more types could be added in the future
<builtin-type> ::= "u"                                      // unit
                 | "v"                                      // void
                 | "y"                                      // byte
                 | "b"                                      // bool
                 | "c"                                      // char
                 | "i" <base-10-number>                     // i64 etc.
                 | "j" <base-10-number>                     // u64 etc.
                 | "f" <base-10-number>                     // f64 etc.
                 | "s"                                      // string
                 | "t"                                      // meta (type type)

<pointer-type> ::= "P"                                      // raw pointer
                 | "P" <type> "E"                           // pointer
                 | "MP" <type> "E"                          // many pointer
                 | "CP" <type> "E"                          // c pointer

<variant-type> ::= "V" <type>* "E"                          // variant type

<tuple-type> ::= "T" <type>* "E"                            // tuple type

<array-type> ::= "A" <base-10-number> <type> "E"            // static array type
               | "D" <type> "E"                             // dynamic array type
<class-type> ::= "C" <path>                                 // class-like types (class, enum, etc.)

<function-type> ::= "F" <function-qualifier>* <return-type> <argument-type>* "E"

<return-type> ::= <type>

<argument-type> ::= <type>

// additional qualifiers for functions including member functions
// more qualifiers should be added in the future
// should not repeat, sorted lexicographically
<function-qualifiers> ::= "C"                               // const member function
                        | "T"                               // thread-safe
                        | "V"                               // virtual function

<unnamed-type-name> ::= "Y" <path> "E" <disambiguator>      // unnamed type
                      | "W" <captures> "G" <return-type> <type>* "E" <disambiguator>  // closure type
<captures> ::= <type>*                                      // types of captured variables
// first unnamed symbol in the scope uses no disambiguator, second gets "_"
// subsequent ones get "(n-2)_" represented in base 62
<disambiguator> ::= ""
                  | <compact-number>

<variadic-arg> ::= "J" <templ-arg>* "E"                     // variadic arguments

<value-arg> ::= "Z" ( <builtin-type> <literal-value> "_"    // built-in types without string, 'n' is used for negative numbers instead of '-', bools use '0' and '1'
              | <base-10-number> "_" <literal-value>        // string literals encoded with length prefix
              | "V" <value-arg>* "E"                        // variant
              | "T" <value-arg>* "E"                        // tuple
              | <function-type> <hex-value> "_"             // function
              | <pointer-type> <hex-value> "_"              // pointer
              | <enum-type> <base-10-number> "_")           // enum
              | <back-reference>

<unscoped-name> ::= <identifier>                            // actual name of a (typical) symbol
                  | <unnamed-type-name>                     // unnamed type or closure
                  | <operator-name>
                  | <special-symbol-encoding>               // special symbols that are created by the compiler
                  | <back-reference>

// special symbols like virtual tables, RTTI, guard variables, structures for generics, etc.
<special-symbol-encoding> ::= "H" <special-symbol-name> "E"
<special-symbol-name> ::= "mc"                              // module constructor
                        | "md"                              // module destructor
                        | "gc"                              // global variable constructor
                        | "gd"                              // global variable destructor
                        | "ic" <function-type>              // implicit constructor (any type: class, tuple, ...; the preceding mangled type disambiguates)
                        | "dc" <function-type>              // default constructor (any type: class, static array, tuple, ...; the preceding mangled type disambiguates)
                        | "cc" <function-type>              // default copy constructor (any type: class, static array, tuple, list; the preceding mangled type disambiguates)
                        | "dd" <function-type>              // default destructor
                        | "ts" <function-type>              // toString method
                        | "length" <function-type>          // length method
                        | "push" <function-type>            // push method
                        | "pop" <function-type>             // pop method
                        | "ba" <function-type>              // box alloc
                        | "bf" <function-type>             // box free
//                      | ...                               // @future: virtual tables, generic structures, named parameter tables, guard variables, ...

<back-reference> ::= "B" <compact-number>                   // reference to a previously defined node

// numbers are encoded with the following scheme:
// 0 is encoded as "_"; other numbers get one subtracted from them
// and the result is encoded in base 62 using as digits 0-9, a-z, A-Z
// e.g.: 0 -> "_", 1 -> "0_", 11 -> "a_", 62 -> "Z_"
<compact-number> ::= "0-9a-zA-Z"* "_"

// Operator names are transliterated character-by-character rather than looked up as whole
// strings: operator names admit arbitrary Unicode (see the lexer's `operator_start`/
// `operator_continue` character classes), so a table keyed on entire operator strings could never
// be complete. A compound operator is just the concatenation of its characters' tags, e.g.
// `+*` -> "plml", `==` -> "eqeq", `<=` -> "lteq" -- no per-combination table needed.
//
// <operatoriness> is required (not inferred from operand count) because e.g. a unary prefix
// `++a` and a unary suffix `a++` have identical arity/types and would otherwise mangle identically.
<operator-name> ::= "O" <operatoriness> <base-10-number> <op-translit-unit>*
                     // <base-10-number> = byte length of the following <op-translit-unit>* run

<operatoriness> ::= "i"                                     // infix (binary)
                  | "p"                                     // prefix (unary)
                  | "s"                                      // suffix (unary, postfix)

<op-translit-unit> ::= <fixed-operator-tag>                    // one recognized operator character
                  | "x" <hex-codepoint> "_"                 // escape for any other codepoint
                     // Not `;`-terminated: `;` isn't a safe symbol character for some
                     // backends (e.g. LLVM). `_` is reused from <compact-number>'s terminator.
                     // Note: `x` must not appear at the beginning of a <fixed-operator-tag>.

// One fixed 2-letter tag per operator *character* (not per operator name).
// More characters/tags can be added in the future.
<fixed-operator-tag> ::= "nt"                               // !
                       | "rm"                                // %
                       | "an"                                // &
                       | "ml"                                // *
                       | "pl"                                // +
                       | "mi"                                // -
                       | "pd"                                // .
                       | "dv"                                // /
                       | "co"                                // :
                       | "lt"                                // <
                       | "eq"                                // =
                       | "gt"                                // >
                       | "qm"                                // ?
                       | "bs"                                // \
                       | "eo"                                // ^
                       | "bt"                                // `
                       | "or"                                // |
                       | "ti"                                // ~

<opt-metadata> ::= "" | <metadata>
// there are no restrictions on <vendor-metadata>, any characters are allowed
<metadata> ::= <metadata-prefix> <vendor-metadata>
<metadata-prefix> ::= "." | "$"

```

Note that named arguments are not part of the mangling scheme, as adding parameter names to mangled names would:
* significantly increase the size of mangled names (slower link time, larger binaries)
* when recompiling make code much less resilient to changes
* hinder ABI stability (e.g. changing parameter name in std would break all binaries)

Instead, when compiling a module to it's object file, the compiler generates a symbol table with names of arguments. When another module imports this one and wants to use named parameters, the table is used to resolve positions at call site and compiler lowers the call to resolved positions.
This approach is used by most languages (Swift, C#, Kotlin, Scala, ...). The metadata can be embedded in the object file, or in a separate file. If embedded we could also make it optional to save space in binaries.
