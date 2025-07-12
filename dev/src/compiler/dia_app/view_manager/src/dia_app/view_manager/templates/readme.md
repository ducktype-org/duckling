@page dia-templates Diagnostic message templates

Message templates (or, actually, *info templates*) are recipes for
converting the raw diagnostic data
emitted by the compiler into a human-readable messages.

They offer methods to embed diagnostic data into the message,
use some basic logic on that data, and, in some cases, even
attach hyperlinks to other messages.

Thanks to them, the compiler itself can be agnostic of the textual
representation of diagnostic messages, a decoupling which facilitates
continuous improvement of said representation independently
of the compiler's source code.

> This document goes through the technicalities of creating and using
info templates. For more information on guidelines about writing the content
of these templates, see the @TODO.

## Anatomy of an info template
A template describes the representation of a standalone, coherent
piece of a diagnostic referred to as an *info*. That info can in turn
contain a header message, a description, a fragment of code
with inline messages (referred to as *pointer messages*),
and a series of, potentially interactive, triggers (called *explore edges*)
representing links to other infos specified by the compiler.

For that reason, the template itself is divided into sections (some optional)
corresponding to each of the aforementioned info components.

@TODO: maybe a screenshot from the web UI could be added here with outlined
sections.

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

@TODO: code, active_from, active_until - do we actually need these and, if so,
mention them here.

~~~~~yaml
metadata:
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
in a View Manager sense (more on that @TODO).

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
header_message: <message>
description: <message>
~~~~~
where `<message>` is a message template (more on those in the
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
    content: <message>
~~~~~
> A scheme for defining a pointer message in the template
file. All pointer messages should be defined under
the `pointer_messages` node at the root of the file.

### Explore edges
Certain pieces of diagnostic data may be represented
as a directed graph. Sometimes it might be useful
for the user to explore such a graph by viewing
a path from the starting vertex and choosing where
to go next or where to go back.

@TODO: we could put some image of this concept
of exploration here in case it's not clear from
the description

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
explore_edges:
  <edge_class>:
    content: <message>
    params: <params>
~~~~~
> A scheme for defining an explore edge in the template file.
All explore edges should be defined under the `explore_edges`
node at the root of the file.

### Macros
To facilitate the process of writing info templates you can
specify non-parametrized macros which may help in avoiding
duplication the the template file. Such macros are basically
just aliases for potentially long and complicated message
templates.

~~~~~yaml
macros:
  <macro_name>: <message>
~~~~~
> A scheme for defining a macro in the template file.
All macros should be defined under the `macros` node
at the root of the file.

## Anatomy of a message template
