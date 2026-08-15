@page dia-file Diagnostic file

The diagnostic file is a JSON representation of arguments passed to the interactive diagnostic system. It contains structured data that will be combined with diagnostic templates (see the \ref dia-templates page) to form messages displayed to the user.

> The diagnostic file provides data, the templates provide phrasing and composition of the message.

# Syntax

The baseline syntax of the diagnostic file is JSON.

> Note: in this document we're denoting an object of type `my_type` as `<my_type>` and an array of objects of type `my_type` as `[<my_type>...]`.

The general structure of the diagnostic file is a single diagnostic:

```
<diagnostic>
```

> Note: earlier versions used a list of "infos"; the current implementation uses a single `diagnostic` object, consistent with `dia_args::Diagnostic`.

Throughout this document tables describe the required (and optional) JSON fields. For example:

> Key   | Value      | Optional
> ----- | ---------- | --------
> type  | `"blob"`   | no
> name  | `<string>` | no
> hobby | `<string>` | yes

describes an object with a required field `type` with value `"blob"`, a required field `name` with value of type `string` (so it can be **any** string), and an **optional** field `hobby` with value of type `string`.

# 1. Top-level objects

## `diagnostic`

Represents a single diagnostic together with all messages that can be attached to it.

This maps directly to `dia::dia_args::Diagnostic`.

Key              | Value                         | Optional
---------------- | ----------------------------- | --------
main_message     | `<message>`                   | no
linked_messages  | `<linked_messages>`           | no

## `linked_messages`

A dictionary containing messages that can be attached to the `main_message`. Keys are of type `string` (message IDs) and values are of type [`message`](#message).

# 2. Messages

## `message`

Represents a single message. This is the JSON counterpart of `dia::dia_args::Message`.

Key            | Value                           | Optional
-------------- | ------------------------------- | --------
metadata       | `<metadata>`                    | no
params         | `<params>`                      | no
explore_links  | `[<explore_link>...]`           | yes
attached_messages | `[<message_id>...]`          | yes

> Note: `attached_messages` is a list of message IDs. Those IDs must be present as keys in the top-level `linked_messages` object.

## `message_id`

A message ID is a `string` that identifies a message. It is used as a key in `linked_messages` and in places where a message is referenced from components.

## `metadata`

Metadata identifies the message or component template (\ref dia-templates) associated with the message.

This maps directly to `dia::dia_args::Metadata`.

Key           | Value        | Optional
------------- | ------------ | --------
template_type | `<string>`   | no
type          | `<string>`   | no
family        | `<string>`   | no
name          | `<string>`   | no

> In practice `template_type` will be one of: `"message"`, `"component"`, `"pointer_message"`.

## `params`

A dictionary with keys of type `string` and values of type [`component`](#2-components). It contains the arguments passed to the template.

## `explore_link`

Represents a single explore link associated with a message. This maps to `dia::dia_args::ExploreEdge`.

Key    | Value         | Optional
------ | ------------- | --------
name   | `<string>`    | no
params | `<params>`    | no

# 2. Components

Components are JSON objects that describe display elements (text, code, links, etc.). All components have a required `type` field, and their structure corresponds to subclasses of `dia::dia_args::Component`.

Some components can be annotated with additional metadata via other components (for example `PointedComponent` uses `PointerMessage`).

Unless explicitly stated otherwise, all fields not marked as optional are required.

## Common rules

- Every component has a `type` field with a `string` value.
- Components are always represented as JSON objects (there is no bare-string shorthand).

## `text` component (`TextComponent`)

Represents simple text.

Key     | Value      | Optional
------- | ---------- | --------
type    | `"text"`   | no
content | `<string>` | no

## `code` component (`CodeComponent`)

Represents a piece of code.

Key     | Value      | Optional
------- | ---------- | --------
type    | `"code"`   | no
content | `<string>` | no

## `code_location` component (`CodeLocationComponent`)

Represents a source code location.

Key    | Value      | Optional
------ | ---------- | --------
type   | `"code_location"` | no
file   | `<string>` | no
line   | `<uint>`   | no
column | `<uint>`   | no

## `start_line` component (`StartLineComponent`)

Represents the start of a new line in a code fragment.

Key    | Value          | Optional
------ | -------------- | --------
type   | `"start_line"` | no
number | `<uint>`       | yes

## `concat` component (`ConcatComponent`)

Represents a concatenation of multiple components.

Key     | Value                    | Optional
------- | ------------------------ | --------
type    | `"concat"`              | no
content | `[<component>...]`       | no

## `pointed` component (`PointedComponent`)

Represents content pointed to by one or more pointer messages.

Key              | Value                         | Optional
---------------- | ----------------------------- | --------
type             | `"pointed"`                   | no
content          | `<component>`                 | no
pointer_messages | `[<pointer_message>...]`      | no

### `pointer_message`

Maps to `dia::dia_args::PointerMessage`.

Key              | Value        | Optional
---------------- | ------------ | --------
pointer_message_id | `<string>` | no
message_id       | `<message_id>` | yes

## `variant` component (`VariantComponent`)

Represents a component that can be replaced with an alternative after user interaction.

Key         | Value         | Optional
----------- | ------------- | --------
type        | `"variant"`   | no
content     | `<component>` | no
alt_content | `<component>` | no

## `link` component (`LinkComponent`)

Represents a link to other messages.

Key             | Value                     | Optional
--------------- | ------------------------- | --------
type            | `"link"`                 | no
target_messages | `[<message_id>...]`       | no
content         | `<component>`             | no

## `evaluated_template` component (`EvaluatedTemplateComponent`)

Represents a reference to another message that should be evaluated as a template and inserted.

Key     | Value         | Optional
------- | ------------- | --------
type    | `"evaluated_template"` | no
info_id | `<message_id>` | no

## `message_id` component (`MessageIDComponent`)

Represents a message identifier used as data inside a component.

Key     | Value         | Optional
------- | ------------- | --------
type    | `"message_id"` | no
info_id | `<message_id>` | no

## `component`

Finally, a `component` is any JSON object understood by `dia::dia_args::Component::fromJson`, that is, one of:

- `text`,
- `code`,
- `code_location`,
- `start_line`,
- `concat`,
- `pointed`,
- `variant`,
- `link`,
- `evaluated_template`,
- `message_id`.

# 3. Laziness and entities

Laziness and entities are handled by the view/state layer and are no longer part of the JSON schema consumed by `diagnostic_arguments`. For current behaviour see the view manager implementation and higher-level diagnostic documentation.
