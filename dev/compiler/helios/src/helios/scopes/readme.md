\page helios-scopes HELIOS Scopes

# HELIOS Scopes API

Note: This is mostly to list operations, see docs for details.

* `QueryRootScopeOf` -- Get module root scope for given module.

* `QueryPrimaryCodeScopeFor` -- Generate HELIOS-scope associated with given PST element.
   Dictates what PST elements have their own scope.

* `queryBodyCodeScopeFor` -- Query a scope, that intuitively represents an element body.

* `QuerySymbolsInScope` -- Get lists of symbols located inside with given scope.

* `LookupInScope` -- Lookups in given scope. Implements main scope-lookup logic.
  Uses `QuerySymbolsInScope`.

* `QueryLookupInScopeAndParents` -- As above, but with parents.

# HELIOS Internal scope operations


* `QueryLinkedScope` -- Returns a scope, that HELIOS can lookup-in, when looking up in given symbol. (works only for some types of symbols).
  Intuitively this returns a scope, programmer would associate with this element when writing `element.some_name`.
  It can work slightly different for different elements.


# Future todos

## Default parameter expression scopes

Scopes of Expression for default parameter values.
Its non trivial, since it will probably get called at callee side, which doesn't work well with lifetimes.

It might require some corner-case if'ing.
Alternatively maybe we want a function per default parameter, that calculates it.

Cool thing is that since move is a zero-cost operation (most of the times), we can just rely on it.

## ...

todo: semantic of QueryPrimaryCodeScopeFor vs scope(SymOf(pst))

This will be set, with follow ups PRs

