@page dia-file Diagnostic file

The diagnostic file is a collection of data the compiler found important
for resolving a compilation error or warning. It contains *raw data*.
That is, no textual messages whatsoever.

> The content of a diagnostic file is combined with the content of
diagnostic templates (see the \ref dia-templates page)
to form a message which can be displayed to the user. The diagnostic file
provides data, the templates provide phrasing and composition of
the message.

## Syntax

The baseline syntax of the diagnostic file is JSON.

> Note: in this document we're denoting an object
of type `my_type` as `<my_type>` and an array
of objects of type `my_type` as `[<my_type>...]`.

The general structure of the diagnostic file goes as follows:
- the file contains a list of info groups,
- each info group represents one diagnostic message and consists of a number
of infos along with metadata,
- each info represents one piece of information in the diagnostic message.

The content of the diagnostic file can therefore
be represented as:
```
[<info_group>...]
```
that is, as an array of info groups.

> Note: there is going to be a lot of tables in the following sections.
Each of them describes the required (and optional) content of JSON objects
of a given type. So, for example, the following table:
>
> | Key     | Value     | Optional  |
> | --      | --        | --        |
> | type    | `"blob"`  | no        |
> | name    | `<string>`| no        |
> | hobby   | `<string>`| yes       |
>
> describes an object with a required field `type` with value `"blob"`,
a required field `name` with value of type `string` (so it can be **any** string),
and an **optional** field `hobby` with value of type `string`.
>
> Here's an example of a JSON object which satisfies the above type:
> ```json
> {
>   "type": "blob",
>   "name": "Bobby"   
> }
> ```

## 1. Infos

### `info_group`
An info group, representing one diagnostic message, consists of
one main info (containing the most general description of the error
or warning) and possibly many secondary infos (some of which may be
displayed at the start - `displayed_secondary_infos` and some after
user interactions).

An info group also contains a dictionary of [entities](#3-entities)
related to its content.

| Key               | Value                 | Optional  |
| --                | --                    | --        |
| main_info         | `<main_info>`         | no        |
| displayed_secondary_infos | `[<info_id>...]` | no |
| secondary_infos   | `<secondary_infos>`   | no      |
| entities          | `<entities>`          | no        |

#### `secondary_infos`
A dictionary with keys of type `string`
and values of type `secondary_info`.

> Note: not all secondary infos mentioned within an info group
must initially be present in this dictionary. Whenever a missing
secondary info is supposed to be displayed to the user, the view
manager module communicates with the language server to fetch
its contents (*).

> (*) This is the goal. Right now there's no integration with
the language server and laziness, though implemented in the view
manager, only fetches the missing content from another place
in the diagnostic file.

#### `entities`
A dictionary with keys of type `string`
and values of type [`entity`](#3-entities).

> Note: not all entities mentioned within an info group
must initially be present in this dictionary. Whenever a missing
entity is needed to displayed something to the user, the view
manager module communicates with the language server to fetch
its contents (*).

> (*) This is the goal. Right now there's no integration with
the language server and laziness, though implemented in the view
manager, only fetches the missing content from another place
in the diagnostic file.

### `info_id`
An info ID is a piece of data which identifies a specific info.
It is represented as a `string` value and acts as a key
in the `secondary_infos` dictionary.

### `main_info`
The main info contains the most general description of a diagnostic message.
It consists of some metadata, a set of parameters for its respective
message template (\ref dia-templates), and an optional code element
to be included in the displayed info.

| Key       | Value                 | Optional  |
| --        | --                    | --        |
| metadata  | `<main_info_metadata>`| no        |
| params    | `<params>`            | no        |
| code      | `<code>`              | yes       |

### `secondary_info`
The secondary infos contain additional information about a diagnostic message.
They are very similiar to main infos, except for their metadata type
and the fact that secondary infos may contain [explore edges](#explore_edge).

> Note: the explore edges are only displayed in infos which have not initially
been displayed.

| Key       | Value                 | Optional  |
| --        | --                    | --        |
| metadata  | `<secondary_info_metadata>`| no   |
| params    | `<params>`            | no        |
| code      | `<code>`              | yes       |
| explore_edges | `[<explore_edge>...]` | yes   |

### Info metadata
Info metadata identifies the message template (\ref dia-templates)
a given info refers to. Main infos can only be of type *error*
or *warning*, while secondary infos may only be one of: *note*,
*hint* or *docs*.

#### `main_info_metadata`
| Key       | Value                 | Optional  |
| --        | --                    | --        |
| type      | `"error"` or `"warning"` | no     |
| family    | `<string>`            | no        |
| name      | `<string>`            | no        |

#### `secondary_info_metadata`
| Key       | Value                 | Optional  |
| --        | --                    | --        |
| type      | `"note"` or `"hint"` or `"docs"` | no     |
| family    | `<string>`            | no        |
| name      | `<string>`            | no        |

### `params`
A dictionary with keys of type `string`
and values of type `component`.

### `code`
A code element represents a piece of code attached to a specific info.
It consists of its location metadata and content of type
[`component`](#2-components).

| Key       | Value                 | Optional  |
| --        | --                    | --        |
| location  | `<location>`          | no        |
| content   | `<component>`         | no        |

> Note that **[text components](#text_component) are not permitted** inside
the `code` element.

#### Code `location`
| Key       | Value                 | Optional  |
| --        | --                    | --        |
| file      | `<string>`            | no        |
| line      | `<uint>`              | no        |
| column    | `<uint>`              | no        |

### `explore_edge`
An explore edge consists of a ID of the info it refers to,
a class edge `name`, and a set of parameters for that edge.

> For more information about explore edges, see the \ref dia-templates
document.

| Key       | Value                 | Optional  |
| --        | --                    | --        |
| info_id   | `<info_id>`           | no        |
| name      | `<string>`            | no        |
| params    | `<params>`            | no        |

## 2. Components
Some components can be annotated with additional
metadata. Namely, a list of `groups` or an `alt_content`.

The `groups` are only used in the context of code
fragments inside infos. A `group` (defined by
a `string` value) represents a set
of components referred to by a *pointer message*
(see \ref dia-templates) with the same name.

The `alt_content` represents an alternative content
of a component which can replace the standard content
after user interaction. It is always accompanied
by a `content` field (the standard content).

### `text_component`
A simple component representing text. Since
the diagnostic file does not contain the message
phrasing, this component is mainly used for
info template logic.

It can either be represented as an object of type
`string`, or an object of the following form:

| Key       | Value             | Optional  |
| --        | --                | --        |
| type      | `"text"`          | no        |
| content   | `<string>`        | no        |

### `code_component`
A simple component representing code.

| Key       | Value             | Optional  |
| --        | --                | --        |
| type      | `"code"`          | no        |
| content   | `<string>`        | no        |
| groups    | `[<string>...]`   | yes       |

### `start_line_component`
A simple component representing the start of
a new line.

When used inside a code fragment,
the optional `number` parameter can be used
to assign a number to that new line.
Code lines *do not* need to be assigned consecutive
numbers, *nor do* all lines need to be numbered.

| Key       | Value             | Optional  |
| --        | --                | --        |
| type      | `"start_line"`    | no        |
| number    | `<uint>`          | yes       |

### `concat_component`
A list of objects of type `component`.
Represents a concatenation of these components
in the provided order.

### `grouping_component`
The grouping component is a component wrapper
which allows the introduction of
[alternative content and groups](#2-components).

> Note that the groups will be assigned to both
the standard content and the alternative content
of this component.

| Key       | Value             | Optional  |
| --        | --                | --        |
| type      | `"grouping"`      | no        |
| content   | `<component>`     | no        |
| alt_content | `<component>`   | yes       |
| groups    | `[<string>...]`    | yes       |

### `entity_component`
The entity component is a component wrapper
which references an [entity](#3-entities)
with the name given in the `refers_to` field.

It also allows the introduction of
[alternative content and groups](#2-components).

> Note that both the groups and the reference
to the entity will be assigned to both
the standard content and the alternative
content of this component.

@TODO is assigning an entity to both content
and alt_content counter-intuitive?

| Key       | Value             | Optional  |
| --        | --                | --        |
| type      | `"entity"`        | no        |
| refers_to | `<string>`        | no        |
| content   | `<component>`     | no        |
| alt_content | `<component>`   | yes       |
| groups    | `[<string>...]`    | yes       |

### `lazy_component`
The lazy component represents a component
which has not yet been evaluated. It contains
a `handle` - a piece of data which can later be used
to retrieve the evaluated content of this
component (*).

| Key       | Value             | Optional  |
| --        | --                | --        |
| type      | `"lazy"`          | no        |
| handle    | `<string>`        | no        |

> (*) This is the goal. Right now there's no integration with
the language server and laziness, though implemented in the view
manager, only fetches the missing content from another place
in the diagnostic file.

### `component`
Finally, a `component` is one of:
- `<text_component>` (*),
- `<code_component>`,
- `<start_line_component>`,
- `<concat_component>`,
- `<grouping_component>`,
- `<entity_component>`,
- `<lazy_component>`.

> (*) See [code element](#code) for an exception to this rule.

## 3. Entities
An entity is any kind of object
which can be referred to by components
present in the scope of an entire info group.
The information it contains may be used
by the view manager (@TODO link) to alter
the metadata of said components.

> This definition is a little abstract, so
let's use an example. Imagine in your diagnostic
message appears a variable `x`. That variable
has a declaration location, last definition location,
a given type etc. You can create an entity for that
variable which will attach notes about all of
the above clues to all occurences of `x`
in the message.

> Note: the name of the entity does not appear
in its JSON representation. Instead, it is specified
as the key in the `entities` field of the info group
which said entity is assigned to.
>
>**Remember** that
the entity name must be unique in the info group
scope.

| Key           | Value         | Optional | Count  |
| --            | --            | --       | --     |
| assoc_infos   | `[<info_id>...]` | yes   | one    |
| `<entity_key>` | `<json>`     | N/A      | many   |

### Associated infos
The `assoc_infos` field contains a list of info
ID's which will be attached to all components
referencing this entity. In practice, it means
that after a user interaction on any of these
components, all of these infos will be opened.

### Other fields
Entities can also contain any number of additional
fields which may or may not be used by the view manager.

> For more information about supported fields, see
the view manager implementation (`display_elements.hpp`).