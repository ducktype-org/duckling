# HELIOS Scopes

# Scope operations

## `QueryRootScopeOf`

module root scope.

## `QueryPrimaryCodeScopeFor`

Maps PST to scopes.
Dictates what PST elements have their own scope.

Currently: 
* For elements that have a scope: it return scope defined for that element,
* For elements, that don't have a scope, but in a way are important in scope structure: it returns QueryPrimaryCodeScopeFor(parent)
* For elements that does
This can be strange, but intuitively it gives the first scope the scope tree, 
walking PST from this element up.

## `QueryIntuitiveCodeScopeFor`

@TODO

## `QuerySymbolsInScope`

Get lists of symbols associated with given scope.

##  `LookupInScope` 

Lookups in given scope. Implements main scope-lookup logic.
Uses `QuerySymbolsInScope`.

## `QueryLookupInScopeAndParents`

As above, but with parents.

## `QueryLinkedScope`

Intuitively this returns a scope, programmer would associate with this element.
It can work slightly different for different elements.

For most symbols it will either panic, or return a scope to lookup in, when
looking up in this element (e.g. for namespace).

## TODOS

todo: semantic of QueryPrimaryCodeScopeFor vs scope(SymOf(pst))

Scopes work as follow:

- **Block/CodeBlockOrStmt**: singe scope containing everything.
  This means that it might be somewhat artificial for functions,
  but this way we can use it for almost everything.
- **Namespace**: transparent?


note: each symbol need a scope. If we don't have one, we should add one.


# Future todos

Scopes of Expression for default parameter values.
Its non trivial, since it will probably get called at callee side.
Maybe we wanta function per default parameter.
Cool thing is that snice move is zero-cost most of the times, we can just rely on it.


# TODO

sanity check, that scope(sym) is correct for all symbols-in-scope


split scope files, so its clear, and simple
