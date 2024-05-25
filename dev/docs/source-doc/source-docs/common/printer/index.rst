=======
Printer
=======

.. Rewritten from readme.md

.. contents::
    :depth: 2
    :local:

This module provides functionality for outputting messages to console. Has tools to help put messages together from smaller parts, like error messages. Additionally contains cool features like changing font and background colors.

.. toctree::
    :caption: Contents:
    :titlesonly:
    :glob:

    src/printer/index.rst
    *

Usage
=====

.. code-block:: cpp
    :caption: Basic usage

    #include <printer/printer.hpp>
    #include <iostream>

    int main() {
        printer::Console console = printer::Console();

        console.add(
            { { { "Hello, World!\n" },
                { "This is the first message." } },
            printer::MessageType::GENERAL}
        );

        console.print(std::cerr);
    }

.. code-block::
    :caption: Console output

    Hello, World!
    This is the first message.

See :doc:`examples` for more details.

Interface
=========

All symbols are in namespace :code:`printer` and come from :code:`printer.hpp`.

TYPE_COUNT
----------

Indicates how many different message types there are.

.. code-block:: cpp

    constexpr usize TYPE_COUNT;

MessageType
-----------

Contains message types, including :code:`ERROR`, :code:`DEBUG`, :code:`NOTE`, :code:`HINT`, :code:`WARNING` and :code:`GENERAL`. Normal message types are enumerated from :code:`0` to :code:`TYPE_COUNT - 1`. Also has type :code:`ALL` used exclusively to change :code:`Console` options.

.. code-block:: cpp

    enum class MessageType;

Console class
-------------

You can add :code:`Message` s to it, change its settings and print its contents to console.

Constructor
^^^^^^^^^^^

.. code-block:: cpp

    Console(usize generalMax = SIZE_MAX, minLevel_t minLevel = defaultMinLevel, maxAmounts_t maxAmounts = defaultMaxAmounts);

* :code:`generalMax`

  Specifies total maximum amount of messages displayed when printing. Helps prevent excess flooding of the console.

* :code:`minLevel`
  
  :code:`minLevel_t` is typedef for :code:`std::array<LevelType, TYPE_COUNT>` (:code:`LevelType` is :code:`int32_t`). Specifies minimal level (of importance) for messages of each type to be displayed. Helps skip less important messages to not flood the console. Default is :code:`0` for all types.

* :code:`maxAmounts`

  :code:`maxAmounts_t` is typedef for :code:`std::array<usize, TYPE_COUNT>`. Specifies maximum amount of each type of messages to be displayed. Helps not display some kind of messages and limit flooding by specific types of messages. Default is :code:`SIZE_MAX` for all types.

Change settings
^^^^^^^^^^^^^^^

When changing a setting, one can use :code:`MessageType::ALL` to change it for all types instead of a specific one.

* :code:`setMinLevel`
  
  Sets :code:`minLevel` for a type.

.. code-block:: cpp

    void setMinLevel(MessageType type, LevelType level);

* :code:`setGeneralMax`
  
  Sets :code:`generalMax`.

.. code-block:: cpp

    void setGeneralMax(usize max);

* :code:`setMaxAmounts`

  Sets :code:`maxAmounts` for a type.

  .. code-block:: cpp

    void setMaxAmounts(MessageType type, usize amount);

add
^^^

Adds a message pack (:code:`std::vector<Message>`) or a single message to the console.

.. code-block:: cpp

    void add(MessagePack pack);
    void add(Message message);

print
^^^^^

Prints added messages using settings' logic to specified ostream. Cerr by default.

.. code-block:: cpp

    void print(std::ostream& out = std::cerr);

clear
^^^^^

Clears all messages from the console. Does not change settings.

.. code-block:: cpp

    void clear();

Color
-----

Enumerates colors for characters and backgrounds. Important to note that colors are inconsistent across terminals. More about it and preview of colors in different terminals `here <https://en.wikipedia.org/wiki/ANSI_escape_code#Colors>`_. There are two special colors - :code:`DEFAULT` and :code:`DEFAULT`. :code:`DEFAULT` is used for :code:`PrinterContent` only and sets the color to :code:`Message`'s default. Don't set :code:`Message`'s color to :code:`DEFAULT`. :code:`DEFAULT` resets color setting to terminal's default.

PrinterContent class
--------------------

Text with color information. Constructors:

.. code-block:: cpp

    PrinterContent(
        const char* str, 
        Color foreground_color  = Color::DEFAULT, 
        Color background_color = Color::DEFAULT);

    PrinterContent(
        PrinterContentText str,
        Color foreground_color  = Color::DEFAULT, 
        Color background_color = Color::DEFAULT);

:code:`PrinterContentText` is just :code:`std::string`. First color is the character color, second one is the background color. If color is omitted not specified, it defaults to :code:`Message`'s default color.

Message class
-------------

Combines multiple :code:`PrinterContent` together into a message.

Constructor
^^^^^^^^^^^

.. code-block:: cpp

    Message(
        std::vector<PrinterContent> list,
        MessageType type = MessageType::GENERAL,
        LevelType level = 0,
        Color foreground_color = Color::DEFAULT,
        Color background_color = Color::DEFAULT)

You can read more about :code:`MessageType` :ref:`here <source-doc/source-docs/common/printer/index:MessageType>`. If omitted defaults to :code:`GENERAL`. :code:`LevelType` is typedef of :code:`int32_t` and notes message's importance to console. If omitted defaults to :code:`0`. First color is :code:`Message`'s default character color, second is :code:`Message`'s default background color. If omitted both default to :code:`DEFAULT` - terminal's default.

add
^^^

Adds :code:`PrinterContent` to the message.

.. code-block:: cpp

    void add(PrinterContent);
    void add(std::vector<PrinterContent>);

print
^^^^^

Prints the message to specified ostream (:code:`std::cerr` by default).

.. code-block:: cpp

    void print(std::ostream& out = std::cerr);

File list
=========

* `printer.hpp <https://github.com/rift-lang/rift-dev/blob/main/dev/common/printer/src/printer/printer.hpp>`_ - interface, all functionalities
* `printer.cpp <https://github.com/rift-lang/rift-dev/blob/main/dev/common/printer/src/printer/printer.cpp>`_ - implementation
* `example.cpp <https://github.com/rift-lang/rift-dev/blob/main/dev/common/printer/examples/example.cpp>`_ - example usage

Notes
=====

We might want to suppress/change the debug messages that show when reaching a limit for :code:`generalMax` or :code:`maxAmounts`.
