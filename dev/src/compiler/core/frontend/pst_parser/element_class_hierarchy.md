# Element Class Hierarchy

Forward declarations are saved in a separate file/files.

Classes can have a static `parse` method that parses any expression, depending on which tokens it encounters.

**Element**  
│ Main class that defines a common parsing and allocator interface.
│ `inline void* operator new(usize size);` - for creating custom allocators in the future
│ `inline void operator delete(void* p)` - as above
│ `virtual [some type] dprint() const = 0;` - Creates printer message of element debug representation
│
├─ **StmtList** final
│  │ Effectively `std::vector<Stmt*>`, but implementing `Element`
│
├─ **Stmt** 
│  │ An element that can appear on its own / A standalone element
│  │  
│  │   
│  ├─ **Expr** final  
│  │  │ Any operator expression.
│  │  │ Specifically handles the lambda operator `=>`, and possibly `:=`, `:`.    
│  │  │ Can introduce new symbols.  
|  |  | Can contain code blocks (lambdas)
│  │  
│  │  
│  ├─ **Decl**  
│  │  │ All items such as functions, loops, code blocks.  
│  │  │ They can declare from 0 to any number of symbols.
│  │  │ This is where most of the logic and a lot of boilerplate (fortunately repetitive) will likely be.
│  │  │  
│  │  ├─ **Fun**
│  │  │  │ Function 
│  │  │  │ ? 
│  │  │ 
│  │  ├─ All declarations (while, for, var?, let?, block, macro, with, loop ...)  
│  │  │  │ @IDEA: If, for example, `for` had a few variants, 
│  │  │  │ it would probably be worth having them as subclasses of a common `For` interface,
│  │  │  │ quite possibly an empty one.
│  │  
│  │  
│  ├─ **Action**  
│  │  │ Structures such as `return 2;`, `break A;`.  
│  │  │ Usually, this will be a keyword followed by an `Expr`.  
│  │  │  
│  │  ├─ **Return**
│  │  ├─ **Break** 
│  │  ├─ **Continue** 
│  │  ├─ **Redo** 
│  │  ├─ **Exit** Do we want this type? 
│  │  ├─ ...
│  │  
│  │  
│  ├─ **Attr** final  
│  │  │ ? Maybe `NotStmt`?
│  │  │ Single attribute `@name(params)` or `@name`  
│  │  
│  │  
│  ├─ **AttrList** final  
│  │  │ Effectively `std::vector<Attr>`, but implementing `parse` - probably uses a generator
│  │  
│  
├─ **NotStmt**
│  │ A class serving to group together building blocks for elements that do not appear independently.
│  │  
│  ├─ **OptionalName** 
│  │  │ Reads an identifier if present, otherwise reads nothing. Saves its state.  
│  │
│  ├─ **ParamList**
│  │  │ List of parameters (of a function, macro, attribute, ...)  
|  |
│  ├─ **ArgList**
│  │  │ List of arguments (of a function, macro, attribute, ...)
|  | 
|  ├─ **CodeBlock**
|  |  | ? maybe this is a `Stmt`?
|  |  | Reads `StmtList` inside `{}`.
|  |
|  ├─ **CodeBlockOrStmt**
|  |  | ? maybe this is a `Stmt`?
|  |  | Reads `StmtList` inside `{}`, or a single `Stmt` inside nothing.
|  |
|  ├─ Any `NotStmt` like fragments of a for-loop



**Generators**
| ? Should they inherit from `Element` -- do they have a common interface ?
| Generator interface, like `template<...> class ParseInOrder`, or `template<T, Separator> class ListOf`
| @IDEA: Maybe not classes?
| Using generators, we still write a new class in the `Element` hierarchy. 
| We rather want to use generators a lot.
| It can contain generators as fields, variables, ...  
|
├─ **ElemArray<T, uint32_t count>**
├─ **ElemList<T>**
├─ **Optional<T>**
├─ **ElemTuple<T...>**
├─ **SimpleExprAction<Key>**
├─ @IDEA: Some things for slicing tokens into fragments
├─ @IDEA: A list, but handling a separator (e.g. `,`). Could be useful for later introducing `For A(), B(), C() {}` and similar.
├─ ...
