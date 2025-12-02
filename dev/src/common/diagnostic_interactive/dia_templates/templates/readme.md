@page dia-templates Diagnostic templates

Diagnostic templates (or, actually, *info templates*) are recipes for
converting the raw diagnostic data
emitted by the compiler into a human-readable messages.

They offer methods to embed diagnostic data into the message,
potentially using some basic logic on that data to alter the content
of the diagnostic message.

Thanks to the templates, the compiler itself can be agnostic of the textual
representation of diagnostic messages, a decoupling which facilitates
continuous improvement of said representation independently
from the compiler's source code.

> This document goes through the technicalities of creating and using
info templates. For more information on guidelines about writing the content
of these templates, see the \ref dia-guidelines page.

## Anatomy of an info template
A template describes the representation of a standalone, coherent
piece of a diagnostic referred to as an *info*. That info can in turn
contain a header message, a description, a fragment of code
with inline messages (referred to as *pointer messages*),
and a series of, potentially interactive, triggers (called *explore edges*)
representing links to other infos specified by the compiler.

For that reason, the template itself is divided into sections (some optional)
corresponding to each of the aforementioned info components.


### Metadata
> Note: this section is **mandatory**.

Every kind of info (and thus, every info template) can be identified by
a triple of names: `(type, family, name)`.

The `type` is one of:
- `error`,
- `warning`,
- `note`,
- `hint`,
- `docs`.

The `family` refers to a group of templates within a given `type`.

The `name` identifies the template within a given `family`.

The whole triple is required to be reflected in the directory structure
inside the template directory.

The metadata also includes information about the range of versions the template
was (or has been) in use, as well as an *info code* - an integer which,
along with the aforementioned `type` can also be used to identify the template.

> In diagnostic messages you often see text like `error[E1001]: ...`.
The `E` in square brackets means an `error` type of the info and the number
`1001` is the *info code*. Together, they uniquely identify the used info
template.
>
> Note: the mechanism for global assignment of info codes has not yet been
developed. When you design an info template, manually make sure to pick
an info code which has not yet been taken.

~~~~~yaml
metadata:
  template_type: "message / component"
  type: "note"
  family: "declaration"
  name: "variable_decl"
  code: "1001"
  active_from: "0.0.1"
  active_until: "now"
~~~~~
> An example of a template's metadata section. This template file must reside
inside the `note/declaration/` directory and be named `variable_decl.yaml`.

### Parameters
> Note: this section is **optional**.

Every template can be parameterized by a set of arguments passed down
by the compiler and each such argument is required to be a *component*
in a View Manager sense (more on that in the \ref dia-file document).

In short, a *component* is a piece of text with metadata which can affect
its display style and introduce interactivity. In particular, a component
might be subject to *in-place transformations* which means that after some user
interaction its textual content might change.

Due to the wide range of transformations that a parameter can undergo,
it is required to create a specification of all parameters
used in a template. Each parameter specification must include a `description`
field documenting its purpose.

> Parameter descriptions are **essential** to creating valid info templates.
They serve as a specification of what the compiler is expected to pass down
to the template and can convey crucial, nontrivial information about
the parameters' allowed behaviour (e.g. by limiting the use of in-place
transformations in certain delicate cases).

Some template parameters in the specification can be marked as optional.

~~~~~yaml
params:
  operator:
    description: "The operator which does not match."
  left_type:
    description: "Type of the left operand."
  right_type:
    description: "Type of the right operand."
  is_static:
    description: "Whether the error is displayed in a static message."
  has_expanded_left_type:
    description: "Whether the type of the left operand can be expanded."
  expanded_left_type:
    description: "Expanded type of the left operand."
    optional: true
  has_expanded_right_type:
    description: "Whether the type of the right operand can be expanded."
  expanded_right_type:
    description: "Expanded type of the right operand."
    optional: true
~~~~~
> An example of a template's parameters specification. The `expanded_left_type`
and `expanded_right_type` parameters are optional. That is, they do not need
to be passed down by the compiler.

### Main messages
There are two main messages displayed within an info:
- a header message (required),
- a description (optional).

Both of these are defined at the root level of an info template file
in the following way:
~~~~~yaml
header_message: <message_components>
description: <message_components>
~~~~~
where `<message_components>` is a message template (more on those in the
[**Anatomy of a message template**](#anatomy-of-a-message-template) section of this document).
The `description` field is optional.

### Pointer messages
> Note: this section is **optional**.

An info template may define a set of messages to be displayed inside
the code fragment provided by the compiler (aka *pointer messages*,
because the *point* to their respective code fragment's segments).

Each pointer message is identified by a *group name* (referring to the group
of components that it points to) and must be assigned:
- a priority (a natural number, where a lower number means higher priority;
the UI may use that information for ordering pointer messages, should they
be displayed at the same location),
- a type (one of the info types mentioned in subsection
[**metadata**](#metadata); in some UI's a pointer message may be displayed
differently based on what type of information it conveys),
- content (a message template).

~~~~~yaml
pointer_messages:
  <group_name>:
    priority: <priority>
    type: <type>
    content: <message_components>
~~~~~
> A scheme for defining a pointer message in the template
file. All pointer messages should be defined under
the `pointer_messages` node at the root of the file.

### Explore edges
> Note: this section is **optional**


Certain pieces of diagnostic data may be represented
as a directed graph. Sometimes it might be useful
for the user to explore such a graph by viewing
a path from the starting vertex and choosing where
to go next or where to go back.


For these scenarios you can use explore edges
in your info templates.

Vertices of such a graph are infos and their respective
explore edges are the outgoing edges of their corresponding
vertices.

Since the outgoing degrees of vertices are unbounded,
the templates of explore edges define *classes* of edges
and not every individual one (otherwise the number
of outgoing edges for a given info template would need
to be bounded or even constant).
The compiler may then provide information about multiple
edges of the same class, all of which are to be displayed
according to that class' explore edge template.

Each edge class may define a set of parameters
(specified in the same way as in the
[**parameters**](#parameters) subsection)
it accepts and use these parameters, alongside
the global info parameters in its `content` field.

> Note that in case of a name conflict, the edge class
parameters shadow the global info parameters.

Since explore edges' primary objective is to provide
interaction on an info graph, all metadata of their
contents is dropped. That means, in particular, that
**no other interaction** will be available on the displayed
explore edge's text.

~~~~~yaml
explore_links:
  <edge_class>:
    content: <message_components>
    params: <params>
~~~~~
> A scheme for defining an explore edge in the template file.
All explore edges should be defined under the `explore_links`
node at the root of the file.

### Macros
To facilitate the process of writing info templates you can
specify non-parametrized macros which may help in avoiding
duplication the the template file. Such macros are basically
just aliases for potentially long and complicated message
templates.

~~~~~yaml
macros:
  <macro_name>: <message_components>
~~~~~
> A scheme for defining a macro in the template file.
All macros should be defined under the `macros` node
at the root of the file.

## Anatomy of a message template

A message template (`<message_components>`) is represented as a tree, where each node
introduces new content or functionality to the message. There are a couple
of node types, depending on their purpose:
- text node,
- concatenation node,
- parameter node,
- macro node,
- matching node.

All message templates are evaluated lazily (thanks to the matching node,
not all branches of the template tree have to be evaluated), which is crucial
for handling optional template parameters.

### Text node

The text node is just plain text. It does not introduce any metadata.
Text nodes are leaves in the message template node tree.

~~~~~yaml
"Example text node in quotes."

Example text not in quotes.

>
Example text
that is split
across multiple lines
for readability.
~~~~~
> Examples of text nodes. Because the general format of the template file
is YAML, the text can be specified with and without quotes. It can also
be split into multiple lines.

### Concatenation node

The concatenation node, as the name suggests, concatenates multiple nodes.
It is represented as a YAML list.

~~~~~yaml
- "The parameter is "
- param: "my_parameter"
- "."
~~~~~
> Example of a concatenation node comprising of a text node, a parameter node,
and another text node.

### Parameter node

<The parameter node, upon evaluation, is replaced by the template parameter>
it refers to which has been passed down by the compiler.

Evaluating a missing optional parameter results in an error.
However, because of lazy and conditional template evaluation
(see [**the matching node**](#matching-node)),
it is possible for such a node to exist in the message template tree
provided it does not get evaluated.

When evaluated inside an edge class' message template, and if there are two
parameters of the same name (one global for the entire info and one local
for this edge class), the parameter node is replaced by the local parameter
(local parameters shadow the global ones).

~~~~~yaml
param: <param_name>
~~~~~
> A scheme for defining a parameter node.

### Macro node

The macro node, upon evaluation, is replaced by the content of the macro
it refers to.

~~~~~yaml
macro: <macro_name>
~~~~~
> A scheme for defining a macro node.

### Matching node

The matching node introduces logic to the message template evaluation.
Upon evaluation, the textual content of the `case` field (plain text
without any metadata) is evaluated and then matched with all cases
specified in the `of` field. Once a match is found, all other cases are
discarded and only the content of the matching one is evaluated.

A few things to note about the matching algorithm:
- cases are divided into *exact cases* and *class cases*,
- *exact cases* are matched first, in order of appearance, only later
*class cases*, also in order of appearance, except for the `[other]` case,
which is considered last,
- *exact cases* are matched exactly with the value of the `case` field,
- *class cases* are matched more broadly, depending on the logic associated
with a given class,
- *class cases* are specified in square brackets,
- `[other]` is a special class case, required in every matching node,
which always produces a match.

> Currently, there is only one class case - `[other]`. More are to be
introduced when needed.

~~~~~yaml
case: <message_components>
of:
  <case_1>: <message_components>
  <case_2>: <message_components>
  "[other]": <message_components>
~~~~~
> A scheme for defining a matching node. Any number of cases can be introduced.
The `[other]` class case is required.
