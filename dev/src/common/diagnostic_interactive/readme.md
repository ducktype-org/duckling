# Diagnostic Interactive

## Description

The main flow of the new diagnostic module uses **templates** for defining the message text content.

The compiler has to generate the arguments for those diagnostic templates.

```
+-------------------------+
| Message builder Library |
+--------+----------------+
         |
         v
+-----------------------+      +-------------------------+
| Diagnostic Arguments  |      |  Diagnostic Templates   |
+-----------------------+      +-------------------------+
         |                                |
         +--------------------------------+
                                 |
                                 v
                         +-------------+
                         |  Template   |
                         | Evaluation  |
                         +------+------+
                                |
                                v
                      +-------------------+
                      | Diagnostic_State  |
                      +---------+---------+
                                |
                                v
                         +-------------+
                         |   Terminal  |
                         |    View     |
                         +-------------+
```

## Directories

* `./src/diagnostic_interactive` - the message builder library
* `./core` - the core library with template deserialization, diagnostic arguments file structures, template evaluation and view construction
* `./term_ui` - the library that takes a `terminal view` and prints it statically to the output

## Interactivity

In the new Diagnostic module there are 2 interactivity elements:

1. **Links** - the message can contain links to other messages.

This include **explore links** which are added at the bottom of the message. 
Usually the content of the error messages is specified by the template.
But in this case we want to have a dynamic number of links specified by the diagnostic arguments file.
The **explore links** can have parameters and are displayed at the bottom of the messages, like:

```
Explore more:
* See the failed candidates. (link)
* Show me the function declaration place.
* ... 
```

Links can appear anywhere in the text or code.

2. **Alternative elements** - the alternative view of the code subsections. 

Can expand / hide code elements on the code snippet, for example

```
 1 | var original_variable = 30;
 2 | alias alias_variable = original_variable;
...
 20| var x = alias_variable;
```

to:

```
 1 | var original_variable = 30;
 2 | alias alias_variable = original_variable;
...
 20| var x = original_variable; # <--- here
```


## Message builder library

It specifies the arguments to create a **diagnostic arguments file**.
The library have some predefined set of utilities to help with this task.

When having difficulty with instruction how to use this class search for the use cases in the source code.

## Diagnostic arguments

This is a JSON-serializable and parsable file that contains the arguments for the template file.

For more info about it's structure see:
[diagnostic arguments file](./core/readme.md)

## Diagnostic templates



## Glossary

* **pointer message**
* **entity**
* **diagnostic arguments**
* **components**