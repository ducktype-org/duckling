# Viewer

## Overview
Let a *component* be any element or group of semantically connected elements
which can be sent over to GUI as a part of the interface data. These components
are represented as classes in our codebase.

The component structure is compositional.
That is, every component uniquely owns its subcomponents.
That way, components form a tree which we call a *view*.

Every component can be converted into a JSON to be sent to GUI.

Some components are interactive (they're called *interactive components*)
and can be queried by GUI using Action API.

Every component has a unique ID among its siblings. That way, each component
can be uniquely identified by an ID path from the root of the view.

The root of the view contains a list of *panels* (a fancy term for a top-level
component) which can be displayed independently next to each other.
> Note: we might either think about ordering (and reordering) of these panels
as queries to GUI or as treat it as the GUI's responsibility. In the second
scenario, we might hint some *priority* for each panel to be displayed in
(though then an action of the form "open a panel right below this one" becomes
difficult to achieve). Additionally, GUI might decide to display some panels
*to the side* (e.g. in a browser) - it is our general philosophy that our library
takes care of *collecting and grouping* data and a GUI takes care of *displaying*
it (deciding local positions of components) in the most convenient way for the user.

## Note on compositionality
Since we want to ensure compositionality, actions cannot inherently change
the type of the component acted upon. For that purpose, a component can
implement its multiple variations in the following ways:

1. Using inheritance and virtual methods (parents hold pointers to children anyways).
2. Using optional fields and variants.

## Action API
Let a *component path* be an array `[n_1, n_2, ..., n_k]`, where `n_i` is the next
child's ID on the path from the view root.

Every interactive component contains a list of actions that can be performed
on it, all uniquely identifiable (locally on that component) by their IDs.

We define an *action path* as a pair `(component_path, action_ID)`, where
there exists an interactive component identified by the `component_path`
having an action with `action_ID` as its ID.

GUI can thus query the `Viewer` by requests of the form:
```json
{
    "component_path": ["n_1", "n_2", ,,, "n_k"],
    "action_id": "ID",
}
```
and the view's compositionality allows for an easy execution of such query.

The response to a query through the Action API is always the new version of the entire component
that the action was finally called upon (the one identified by the `component_path`)
(and, optionally, a new top-level panel to "bubble up" to the surface,
for when we want the action to add a new panel to the root of the tree).

Actions themselves are stored in interactive components as pointers-to-closures,
returning augmented copies of these components under the given action (and, optionally,
the aforementioned top-level panels).

That way we have some optimization, for when the entire tree becomes large
(e.g. holds large lists of possible overloads to some function) and we just
want to execute a small unrelated action. Then we don't have to transfer the entire
tree to GUI, just the modified subtree.

> Note that our library keeps track of the component tree state, so it is in-sync
with GUI and can be queried for actions.
>
Sidenote: where do we actually store the tree? We settled on the following
idea.

Our library gives access to a `Viewer` class, which can be constructed given
a path to the diagnostics file (or LS port). In the constructor, the `Viewer`
constructs a `ResourceManager` passing it its arguments and an empty component tree.

That way we end up with the following schema:
```
/// Viewer:
Viewer {
    Component rootNode,
    ResourceManager manager
}
/// HTTP server:
#include "viewer.hpp"
int main() {
    Viewer v("diagnostics.dia");
    waitForHTTP(
        "start" -> send(v.start()),
        ("action", action) -> send(v.act(action))
    );
}
// Note: the HTTP server is NOT a part of our library. It's just a transparent
// facade for forwarding messages between our example UI in Vue and our library.
```

Why do we need a `ResourceManager`? Because it is convenient to abstract away
the way we get our error data (then integrating with a LS only changes the ResourceManager
code and not the `Viewer` - viewer still executes the same queries on the resource
manager, but the manager lazily makes calls to the LS with some strategies
to execute in order to retrieve the wanted data).

> Note: that means it would be ideal to write `ResourceManager` calls in an
async manner, though we may not have enough time for that. But even then,
the abstraction seems quite important.

Some more thoughts on the topic in [this readme](../../README.md).