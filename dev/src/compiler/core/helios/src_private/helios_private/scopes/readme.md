# HELIOS Scopes

Note: This is mostly to list operations, see docs for details.

* `QueryRootScopeOf` -- Get module root scope for given module.

* `QueryPrimaryCodeScopeFor` -- Generate HELIOS-scope associated with given PST element.
   Dictates what PST elements have their own scope.

* `queryBodyCodeScopeFor` -- Query a scope, that intuitively represents an element body.

* `QuerySymbolsInScope` -- Get lists of symbols located inside with given scope.

* `QueryLookupInScope` -- Symbols directly in given scope that may provide the name, including the
  not yet resolved `using`/`import` symbols. Uses `QuerySymbolsInScope`.

* `QueryLookupFromScope` -- Main scope-lookup logic. Searches a scope together with everything its
  `using`/`import` symbols bring in (resolved with `QueryResolveUsingImport`), visiting every scope
  at most once, so recursive usings and imports work. Walks the enclosing scopes in a loop: parent
  scopes, then root scopes of parent REPL modules (symbol visibility across REPL statement history),
  then the implicit prelude. `LookupMode` decides whether it stops at the first scope that found
  anything or collects the symbols from all of them.

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

