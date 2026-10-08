# Diagnostic

For trouble with new Diagnostic library usage search for implemented examples in the compiler.

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

* `./src/diagnostic` - the message builder library together with the source position, location and highlight position types
* `./core` - the core library with template deserialization, diagnostic arguments file structures, template evaluation and view construction
* `./dia_templates` - the diagnostic template files and their access layer (filesystem or embedded)
* `./term_ui` - the library that takes a `terminal view` and prints it statically to the output
* `./lsp_ui` - the library that converts a diagnostic into its LSP representation

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
 2 | using original_variable as alias_variable;
...
 20| var x = alias_variable;
```

to:

```
 1 | var original_variable = 30;
 2 | using original_variable as alias_variable;
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
[diagnostic arguments file](./core/diagnostic_arguments_file.md)

## Diagnostic templates

This is a YAML file that defines the templates.

For more info about it's structure see:
[diagnostic template file](./core/diagnostic_template_file.md)

## Guidelines

There are also guidelines for writing good and infromative error messages,
see [here](./core/guidelines.md)


## Glossary

* **pointer message** - a part of code that is highlighted and the highlight have a message
* **entity** - a mechanism to automatically add links to all the element related to one entity, like variable name, type name in the code snippet
