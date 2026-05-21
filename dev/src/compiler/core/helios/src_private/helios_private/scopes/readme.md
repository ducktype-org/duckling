# HELIOS Scopes

Note: This is mostly to list operations, see docs for details.

* `QueryRootScopeOf` -- Get module root scope for given module.

* `QueryPrimaryCodeScopeFor` -- Generate HELIOS-scope associated with given PST element.
   Dictates what PST elements have their own scope.

* `queryBodyCodeScopeFor` -- Query a scope, that intuitively represents an element body.

* `QuerySymbolsInScope` -- Get lists of symbols located inside with given scope.

* `LookupInScope` -- Lookups in given scope. Implements main scope-lookup logic.
  Uses `QuerySymbolsInScope`.

* `QueryLookupInScopeAndParents` -- Special REPL handling: at root scope, checks if module is a REPL module with a parent.
  If yes, continues lookup in parent module's root scope, enabling symbol visibility across REPL statement history.

# HELIOS Internal scope operations

* `QueryLinkedScope` -- Returns a scope associated with given HELIOS Symbol in terms of lookup.
  For example: 

  * For namespaces it returns namespace body-scope
  * For wildcard usings it returns `QueryLinkedScope` of the symbol using is pointing to.

  HELIOS uses it as an auxiliary query, in lookup implementation. 
  Note: this query will likely be deleted in the future, after the Scope Refactor.


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

## Related Documentation
- [REPL Module](../../../../../repl/readme.md)

