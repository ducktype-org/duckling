# Printer Requirements

## Module for printing messages to the console.

### The module implements:

1. `typedef uint32_t LevelType`

2. `enum MessageType` (Error = 0, Debug = 1, Note = 2, Hint = 3, ...)
    It should have a defined underlying type; conversion to the appropriate int should be possible (this can be a function).

3. `constexpr usize typeCount` defining the number of message types (the code should assume that this value may change).

4. `enum Color`, a variable `Color defaultColor = White`.

5. A `PrinterContent` type, which for now is simply equal to `std::string` (so, a `typedef` for the time being).

6. A `ColoredContent` type, which holds a message of type `PrinterContent` and a color of type `Color`.

7. A function (potentially a constructor) `ColoredContent paint(Color color, PrinterContent str)`.
    Additionally, it exposes functions like `default, red, blue, ...`, which act like paint, but with a specific color.

8. A `Message` type, which possesses a type of `enum MessageType` and a level of `LevelType`.
    The message can be constructed from `std::initializer_list<ColoredContent>` or `std::initializer_list<PrinterContent>`.
    (`PrinterContent` elements are automatically converted to `ColoredContent` with the default color).
    The message maintains a list of `ColoredContent` (likely `std::vector<ColoredContent>`).
    In the future: construction from `std::initializer_list<std::variant<ColoredContent, PrinterContent>>`.

9. A `MessagePack` type, which is effectively a vector of messages (for now, it can be a `typedef`).

10. A `Console` type, to which `MessagePack` can be added.
    It maintains:
    
    * (1) a vector of `MessagePack`.
    * (2) a `LevelType` value for each message type, specifying the minimum level for that message type.
        (an easy refactor to 'maximum' should be possible)
        (likely `std::array<LevelType, typeCount>`).
    * (3) A `generalMax` value of type `usize` defining the total maximum number of messages.
    * (4) Values of type `usize` defining the maximum number of messages for each type.
        (likely `std::array<usize, typeCount>`).

    It also has a `printErr` method, which prints all messages to `stderr` (with colors), but:
    - a single MessagePack is treated as multiple separate messages contained within it.
    - it skips the message if the message type should be ignored due to (2).
    - it stops if the number of printed messages exceeds `generalMax`.
    - it skips the message if the number of printed messages of that type exceeds the corresponding value.
    - the two cases above result in the printing of an additional short message, not counted towards the limits, explaining what happened.
    
    Additionally, it has a `clear` method, which removes all messages held by the console.

11. In the future: type `MessageTemplate`.

12. In the future: move semantics where appropriate.

13. In the future: optimizations...

### From this, it exposes (in .hpp):

...
