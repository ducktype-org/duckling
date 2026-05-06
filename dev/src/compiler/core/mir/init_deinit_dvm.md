# Init and deinit propagation from MIR to the DVM Backend

## Motivation 
We want to have the init and deinit created in correct places, so that we have lower maximal stack size in the VM and we can track the lifetime of variables, to detect when we use a variable that has deallocated from the stack:

```c
let p: ptr i64;
{
    let x = 20;
    p = &x;
}
{
    let z = 30;
    print(p); // in C/C++ we would print 30, but we would like to throw an error
    // in order to do so, we have to add `init/deinit` denoting that a variable
    // goes out of scope and it's scope starts
}
```

Note that it's not the same thing as constructos/destructors, as a constructor for example can be a simple assignment `x = 20` and we have to add init before that and add deinit after the destructor.

```dbc
init(x)
construct(x)
...
destruct(x)
deinit(x)
```

My initial solution would be to just use the places where we construct and destruct
the variable and add `init` before the constructor and `deinit` after the destructor,
but that idea has many problems:

## Problems with using constructor and destructor places for `init/deinit`

### The destructors of the variables that has not been constructed yet

```c
{
    var x = 0; // construct(x)
    if (...) {
        // destruct(x)
        // destruct(y)
        // here we would add `deinit` for both
        // x and y, but the `y` has not been constructed yet
        return 0; 
    }
    var y = 0; // construct(y)
    // destruct(x)
    // destruct(y)
    return 0;
}
```


### Temporary variables scopes overlap

Temporary variables generated for example for expression codegen start and end overlapping 
with each other.

```c
var x = 5 + 4 + 3;
```

Generated MIR:

```mir

tmp = 5 + 4 (construct(tmp))
x = tmp + 3 (construct(x), destruct(tmp))
```

Because the `construct/init` should be before the instruction itself
and the `destruct/deinit` should be after the instruction itself, 
we would have in DBC:

```dbc
init tmp
tmp = 5 + 4 
init x
x = tmp + 3 
deinit tmp
...
deinit x
```

But the `init`/`deinit` are paired and are on the stack, so the `deinit tmp` gets paired with the `init x`.

```dbc
* init tmp
| tmp = 5 + 4 
|  * init x
|  | x = tmp + 3 
|  * deinit tmp
| ...
* deinit x
```



## What we use instead

Instead of generating `init`/`deinit` at constructor/destructor call 
we look at the places where the variables goes into the scope and out of scope.
It is not the start of their lifetime, as their lifetime is between the `construct/destruct`,
but it's a place where we prepare the space for them on the stack memory.

Example:
```c
{
    var x = 0; // construct(x), scopeStart(x), scopeStart(y)
    if (...) {
        // destruct(x)
        // destruct(y)
        // here we would add `deinit` for both
        // x and y, but the `y` has not been constructed yet
        return 0; // scopeEnd(x), scopeEnd(y)
    }
    var y = 0; // construct(y)
    // destruct(x)
    // destruct(y)
    return 0; // scopeEnd(x), scopeEnd(y)
}

This also solves the problem with the temporary values.
This `scopeStart`/`scopeEnd` is generated using only MIR scopes,
so no manual changes at all to the expression/stmt MIR codegen are needed.
```

## Other problems with lifetimes

### Return temporary value

When we return from a variable:

```c
var x;
return x;
```

Where to put the `destructor` for the `x`? We can't do that 
before the `return` as we need the return value,
but we also can't do that  after the return.

The solution the MIR does is it creates a temporary:

```c
var x;
return_tmp = x;
destructor(x);
return return_tmp;
```

And does not create a destructors for the `return_tmp`.

**Solution**

The solution to this is actually add the mapping from the `return_tmp`
to the `ret0`, which is a special DVM place denoting the return value of the function.
This place is valid for the entire function and requires no `deinit`/`init`,
and the only used is it move something to it.

So we denote in MIR that the the `return_tmp` should not have the `scopeBegin`
`scopeEnd` nor the `construct`/`destruct` flags. This way the destructor,
constructor, init/deinit are not created for that variable. 

### Destructor of the variable needed for the branching instruction

The problem is:

```c
if (x > y + 1) {...}
```

```mir
tmp1 = y = 1
tmp2 = x > tmp1; 
cond_tmp = tmp2 scopeStart(cond_tmp)
destruct(tmp2)
# we can't deinit the tmp here, so where?
branch cond_tmp, blockTrue, blockElse; (scopeEnd(cond_tmp))
```

We don't know where to `deinit` the cond_tmp from the condition, as it is needed by the branch instruction.
The good solution, that may be used in the future is to generate the `deinit` in all of the successor blocks.
If they differ in lifetimes, we create a new block in between the jumps only for the `deinit` instructions.

But we don't have this problem in DBC, as one branch instruction is translated into multiple instructions
with the `condition flag` as the source of information for the branching

```dbc
tmp1 = y + 1
tmp2 = cmpGreaterThan x, tmp1
init cond_tmp
cond_tmp = tmp2

destructor(tmp2)
deinit tmp2
~~~ start of the branch instr ~~~
cmp cond_tmp, 1;
deinit cond_tmp # we can add deinits here
jmpIf blockTrue
jmpIf blockElse
~~~ end of the branch instr ~~~
```

Note that side effect of this is that there are `ScopeEnd` instructions on the branching instructions

```mir
tmp1 = y = 1
tmp2 = x > tmp1;
branch tmp2, blockTrue, blockElse; scopeEnd(tmp1), scopeEnd(tmp2)
```

And they are generated before the actual jump in the DVM bytecode.