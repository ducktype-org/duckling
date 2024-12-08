# HELIOS Scopes

Scope operations:

* `QueryRootScopeOf` -- module root scope
* `QueryPrimaryCodeScopeFor` -- pst element scope
* `LookupInScope` -- for looking up in given scope
* `QueryLookupInScopeAndParents` -- as above, but with parents
* `QuerySymbolsInScope` -- get lists of symbols associated with given scope
* `QueryLinkedScope` (defined in symbols.hpp/cpp) -- given a scope to lookup in, when looking up
  in symbol for which we just lookup in scope.

Scopes work as follow:

- **Block/CodeBlockOrStmt**: singe scope containing everything.
  This means that it might be somewhat artificial for functions,
  but this way we can use it for almost everything.
- **Namespace**: trans



# Future todos

Scopes of Expression for default parameter values.
Its non trivial, since it will probably get called at callee side.
Maybe we wanta function per default parameter