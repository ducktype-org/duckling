### mangling scheme

This is the most up-to-date version of the mangling scheme. If any changes are made,
either in the scheme or it's implementation, they should be reflected here.

```rust

<mangled-symbol-name> ::= <language-prefix> <scheme-version> <encoding> <opt-metadata>
                        | <no-mangling>                     // C linkage (builtins, extern C, special e.g. main)

// note: global identifiers starting with underscore and a capital letter are reserved in C
// Q seems to be free and stands for both query and quack
<language-prefix> ::= "_Q"

<scheme-version> ::= <compact-number>                       // version of the mangling scheme

<encoding> ::= <path>                                       // variables and constants
             | <path> <function>                            // functions
             | <repl-expression-wrapper>                    // REPL expressions

// REPL expression wrappers use simplified mangling for now. @TODO: #1768 decide
// if it's correct.
<repl-expression-wrapper> ::= "__repl_expr_wrapper_" <base-10-number>               // @taw3e8 @todo: no known length

// Same with REPL instruction wrappers.
<repl-instruction-wrapper> ::= "__repl_instr_wrapper_" <base-10-number>             // @taw3e8 @todo: no known length

<path> ::= <path-prefix> <symbol-name>
         | <back-reference>

<path-prefix> ::= "P" <package-name> <module-name>+                 // module-path of the module in a package (note: ends with <symbol-name>)
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
// note: always starts with a digit/"U"
<identifier> ::= <encoding-identifier> <base-10-number> <raw-identifier>
               | <back-reference>
// if the identifier contains a unicode character, it is encoded using punycode
// note: raw-identifiers should start with a letter or an underscore and should consist of letters, digits, and underscores @Taw3e8 @todo: check & enforce this in the compiler
<encoding-identifier> ::= ""                                // no encoding
                        | "U"                               // punycode for encoding unicode characters

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

<templ-arg> ::= <type>                          // @Taw3e8 @todo: in function types we may need to reference the template parameters; consider: "T" <compact-number> 
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
<type-modifier> ::= "N"                                     // const           // @Taw3e8 @todo: to refactor -> conflicts with pointer types?
                  | "M"                                     // unique           // @Taw3e8 @todo: to refactor -> conflicts with pointer types?
                  | "L"                                     // leaking          // @Taw3e8 @todo: to refactor -> conflicts with pointer types?
                  | "R"                                     // reference        // @Taw3e8 @todo: to refactor -> conflicts with pointer types?
                  | "X"                                     // box              // @Taw3e8 @todo: to refactor -> conflicts with pointer types?

// more types could be added in the future
<builtin-type> ::= "u"                                      // unit
                 | "v"                                      // void
                 | "y"                                      // byte
                 | "b"                                      // bool
                 | "c"                                      // char
                 | ""
                 | ""
                 
                 
                 | "i" <base-10-number>                     // i64 etc.                     // @Taw3e8 @todo: use itanium ABI manging for lengths
                 | "j" <base-10-number>                     // u64 etc.                     // @Taw3e8 @todo: use itanium ABI manging for lengths
                 | "f" <base-10-number>                     // f64 etc.                     // @Taw3e8 @todo: use itanium ABI manging for lengths


                 | "s"                                      // string
                 | "t"                                      // meta (type type)

<pointer-type> ::= "P"                                      // raw pointer
                 | "P" <type> "E"                           // pointer
                 | "MP" <type> "E"                          // many pointer
                 | "CP" <type> "E"                          // c pointer                    // @Taw3e8 @todo: "C" prefix conflicts with class type

<variant-type> ::= "V" <type>* "E"                          // variant type

<tuple-type> ::= "T" <type>* "E"                            // tuple type

<array-type> ::= "A" <base-10-number> <type> "E"            // static array type           // @Taw3e8 @todo: consider compact number for length (or base-32/16)
               | "D" <type> "E"                             // dynamic array type
<class-type> ::= "C" <path>                                 // class-like types (class, enum, etc.)

<function-type> ::= "F" <function-qualifier>* <return-type> <argument-type>* "E"

<return-type> ::= <type>

<argument-type> ::= <type>

// additional qualifiers for functions including member functions
// more qualifiers should be added in the future
// should not repeat, sorted lexicographically
<function-qualifiers> ::= "C"                               // const member function       // @Taw3e8 @todo: conflicts with class type
                        | "T"                               // thread-safe
                        | "V"                               // virtual function

<unnamed-type-name> ::= "Y" <disambiguator>      // unnamed type
                      | <closure>              // types of lambda expressions
<disambiguator> ::= <compact-number>               // number of a given unnamed type/lambda in a given namespace/class

<closure> ::= <closure-type> <parameter-name>* "E"
               where <closure-type>.<argument-type>.count == <parameter-name>.count
<closure-type> ::= "W" <captures> <return-type> <argument-type>* "E" <disambiguator>  // closure type
<captures> ::= <type>* "G"                                      // types of captured variables

<variadic-arg> ::= "J" <templ-arg>* "E"                     // variadic arguments

// @Taw3e8 @todo: conider renaming value arg to CTV
// @taw3e8 @todo: fit those into the mangling scheme, and add support for them
/*
CTV
    -bool
    -NumericValue
        std::int8_t
        i16
        i32
        i64
        std::uint8_t
        u16
        u32
        u64
        f32
        f64
    -char
    base::StrID         
    -UnitCTV
    -TupleCTV            // at least 2 values
    tsh::SymbolType
*/

<value-arg> ::= "Z" (
              // built-in types without string; integers and chars are encoded in base-10, 'n' is used for negative numbers instead of '-'; bools use '0' and '1' for false and true; floats are encoded in their hex representation using "0-91-f", high bits first 
               <builtin-type> <literal-value> "_"
              // string literals encoded with length prefix
              | <base-10-number> "_" <literal-value>
              | "V" <value-arg>* "E"                        // variant
              | "T" <value-arg>* "E"                        // tuple
              | <function-type> <hex-value> "_"             // function as a hex address
              | <pointer-type> <hex-value> "_"              // pointer as a hex address
              | <enum-type> <base-10-number> "_")           // enum
              | <back-reference>

<unscoped-name> ::= <identifier>                            // actual name of a (typical) symbol
                  | <unnamed-type-name>                     // unnamed type or closure
                  | <operator-name>
                  | <special-symbol-encoding>               // special symbols that are created by the compiler
                  | <back-reference>

<operator-name> ::= <chain-operator>
                  | <unary-operator-name>                   // inside class; no need for argument type
                  | <binary-operator-name> <type> <type>    // outside class

<chain-operator> ::= "ch" <type> (<binary-operator-name> <type>)* "E"

// special symbols like virtual tables, RTTI, guard variables, structures for generics, etc.
<special-symbol-encoding> ::= "H" <special-symbol-name> "E"
<special-symbol-name> ::= "mc"                              // module constructor
                        | "md"                              // module destructor
                        | "gc"                              // global variable constructor
                        | "gd"                              // global variable destructor
                        | "ic" <function-type>              // implicit class constructor           // @Taw3e8 @todo: all those seem kinda sus...
                        | "dc" <function-type>              // default class constructor            // @Taw3e8 @todo: all those seem kinda sus...
                        | "ds" <function-type>              // default static array constructor     // @Taw3e8 @todo: all those seem kinda sus...
                        | "dt" <function-type>              // default tuple constructor            // @Taw3e8 @todo: all those seem kinda sus...
                        | "dd" <function-type>              // default destructor
                        | "ts" <function-type>              // toString method                      // @Taw3e8 @todo: make sure this should be a special symbol and not a regular method
//                      | ...                               // @future: virtual tables, generic structures, named parameter tables, guard variables, ...

<back-reference> ::= "B" <compact-number>                   // reference to a previously defined node

// numbers are encoded with the following scheme:
// 0 is encoded as "_"; other numbers get one subtracted from them
// and the result is encoded in base 62 using as digits 0-9, a-z, A-Z
// e.g.: 0 -> "_", 1 -> "0_", 11 -> "a_", 62 -> "Z_"
// note: since it starts with any character, it can only appear in ocntexts where it is expected
<compact-number> ::= "0-9a-zA-Z"* "_"

// more operators can be added in the future
// note: inside class; no need for argument type
<unary-operator-name> ::= "ps"                             // +
                        | "ng"	                            // -
                        | "ad"	                            // &
                        | "de"	                            // *
                        | "nu" <identifier>                 // (named unary prefix operator)
                        | "nU" <identifier>                 // (named unary postfix operator)

// more operators can be added in the future
<binary-operator-name> ::= "co"	                            // ~
                         | "pl"	                            // +
                         | "mi"	                            // -
                         | "ml"	                            // *
                         | "dv"	                            // /
                         | "rm"	                            // %
                         | "an"	                            // &
                         | "or"	                            // |
                         | "eo"	                            // ^
                         | "aS"	                            // =
                         | "pL"	                            // +=
                         | "mI"	                            // -=
                         | "mL"	                            // *=
                         | "dV"	                            // /=
                         | "rM"	                            // %=
                         | "aN"	                            // &=
                         | "oR"	                            // |=
                         | "eO"	                            // ^=
                         | "ls"	                            // <<
                         | "rs"	                            // >>
                         | "lS"	                            // <<=
                         | "rS"	                            // >>=
                         | "eq"	                            // ==
                         | "ne"	                            // !=
                         | "lt"	                            // <
                         | "gt"	                            // >
                         | "le"	                            // <=
                         | "ge"	                            // >=
                         | "nt"	                            // !
                         | "aa"	                            // &&
                         | "oo"	                            // ||
                         | "pp"	                            // ++
                         | "mm"	                            // --
                         | "pt"	                            // ->
                         | "cl"	                            // ()
                         | "ix"	                            // []
                         | "cv" 	                        // (cast)
                         | "nm" <identifier>                // (named binary operator)

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
