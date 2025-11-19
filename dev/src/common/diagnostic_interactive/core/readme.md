3 types of interactivity

1. The links

In the text message specified in the template you can have links to other messages.

```
In the Duckling template 

... Some code of the template ...

Show the template instiations place. <- this can be a alink
```

But sometimes you don't know in advance how many link you would like to have.
That's why there are "explore messages" that are placed in the end. They mean
that at the end of the error message you can have:

```
Ambiguous exact candidates.

Explore:
- Explain exact match candidate `foo(x:i64, y:i64)`
- Explain exact match candidate `foo(x:i64, y:i64, z:i64 = 0)`
- Explain exact match candidate `foo(x:i64, y:i64, z: i64 = 0, w:i64 = 0)`
- Show coercion candidates
- Show unmatched candidates
```

These should be all links.

2. The entities

The code snippets can have interactive entities, for example variables or types.

In the error message:
```
var x: i64 = w;
```

W can be an entity. The entites can link to some other note messages, for example place of declaration.


3. The alternative view of the code subsections. 

Can expand / hide code elements on the code snippet, for example

```
 1 | var original_variable = 30;
 2 | alias alias_variable = original_variable;
...
 20| var x = alias_variable;
```

to:

```
 1 | var original_variable = 30;
 2 | alias alias_variable = original_variable;
...
 20| var x = original_variable;
```

Use cases: hide long symbol name, expand symbol name, replace implicit type deduction into explic type declaration, expand macro
