
# First thing that will happen

`PST.lowerToHIR` will be called, which will execute dfs-like walk on elements tree.

What will happen in the process:

  * All `imports will be detected and stored`
  * All declaration names will be changed into `SymbolId`
  * `ScopeId` will be assigned to every code scope
  * Each `Expr` will be converted into one of the following:
    * `Expr`
    * `VarDecl`
    * `LambdaExpr` (@TODO: is it needed?)
    * `CodeBlockExpr` (@TODO: is it needed?)
    * `DotedName` (@TODO: is it needed?)
    * `Name` (@TODO: is it needed?)
  * All attributes will be aggregated and will be given one of the following types (@TODO: is it needed?):
    * `???`
    * `UserDefined`
  * If some attributes will need to be considered at this step, then user-defined attributes cannot shadow builtin ones.

The result will consist of `HIR` class, a structure similar to `PST`.
Unfortunately it will generate a lot of similar and related code in `PST` and `HIR` (@TODO: perhaps it can be avoided or minimized)

`HIR` form will perform the following:

* `using` statement execution
* recursive semantic analysis that will mosty consist of compile-time calculations, type-checking, name-resolutions and expression lowering.

After that further lowering and code flattening will have to occur, which will be, in some way, followed by compilation. 

# Problems:
  * Declaration of symbols visible outside of macro scope (by given macro) seams (at least) hard 

