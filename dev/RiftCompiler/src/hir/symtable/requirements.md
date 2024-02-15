<!-- Deprecated -->

## Implementation of symbol table and scope data types

Implements:

1. type `ScopeId` - id of any scope
1. type `SymbolId` - id of any symbol
1. type `SymTable` - main type 1
1. type `ScopeGraph` - main type 2, contained in `SymTable`

Create new scope:

`ScopeId ScopeGraph::newScope(parent_scope, scope_type?)` - creates new scope, every scope has main parent
`void ScopeGraph::linkScope(from_id, to_id, link type)` - add links between scope (for example with using statement)

Create new symbol:

`SymbolId SymTable::newSymbol(sym_type?, static?, parent_scope?, position?, name?, PtrToSomeData)`

Qnl phase does not use non-parent scope links, considered scopes are only given `scope`, and its ancestors.  
`SymbolId SymTable::qnl(sym_name, scopes...?, position)` -- just one step or all at the same time?

Unl phare take
`ScopeId... SymTable::getVisibleScopes(scope, position)`
`SymbolId SymTable::unl(sym_name, position, scopes...)` -- just one step or all at the same time?


## Assumptions:

`imports` and `using?` are using only QNames 

Rozwiązania:
3. All static symbols at beginning of scope


Rust:
1. `int a = c; int c = 2;`? - works for static `c`
```rust
fn Af(v: i32) -> i32 {
   if v < 0 {return 0;}
   return Bf(v - 1);
}

fn Bf(v: i32) -> i32 {
   return Af(v - 1);
}

fn Tf(v: T) {
   let b = v.a;
}

struct T {
   a: i32
}

fn main() {
   let B:i32 = 2;
   fn foo() {
      let a: i32 = B;
      static B: i32 = 1;
      print!("{}", a); // outputs `1`

   }

   foo();
   Bf(100);
   let p:T = T{a: 2};
   Tf(p);
   return ();
}
```
