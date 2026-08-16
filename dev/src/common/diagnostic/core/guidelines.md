@page dia-guidelines Guidelines for diagnostics

Here is a set of rules which we believe will help design informative,
user-friendly and coherent error messages.

Some of the guidelines
come from research around the topic (if you're interested, you
can find a good start [here](https://amirkamil.com/papers/iticse19.pdf))
and some are in place to maintain a coherent and clean
database of error messages. Certain points are also inspired from
[the rustc style guide](https://rustc-dev-guide.rust-lang.org/diagnostics.html#suggestion-style-guide)
and [clang documentation](https://clang.llvm.org/docs/InternalsManual.html#diagnostic-wording).

> Note: this document assumes that you have read the documentation
of \ref dia-templates. There you can find
technical information about the anatomy of diagnostics (what are
its parts and how these parts are used).

## 1. Language

### *Be an assistant, not a grader*
Remember that the compiler diagnostics' job is to help the user
reach a state in which their code is sound and functions the way
*they* intended. The messages are *guides for the user*, and
**not** *complaints of the compiler*.

Therefore, maintain a positive, polite, and informative tone in your messages.
Specifically, **don't blame the user** for their code.
Think of debugging as a cooperation between the user and the compiler
in which both parties try to agree on a code that meets both of their
expectations.

> Focus on the *reason* of the code being erroneous.
> If that's not possible, use a narrative in which the compiler
> is unable to perform a certain task that the user
> asked it to do (don't impersonate the compiler, though).

#### Blacklist of words
There are some popular words, which, according to the aforementioned
specification, should definitely be avoided. Here are some of them,
for reference.

- `illegal`
- `invalid`
- `incorrect`
- `wrong`
- `bad`
- `erroneous`

#### Examples
- Sometimes you can simply omit a blacklisted word.

Instead of:
```
invalid use of a reserved keyword `let`: cannot be used as an identifier
```
you can write:
```
symbol `let` is a reserved keyword and cannot be used as an identifier
```
- Sometimes the word can be replaced by an alternative, more informative one.

Instead of:
```
incorrect argument type in a function call
```
you can write:
```
argument type mismatch in a function call
```

### *Be specific*
Use specific language construct names (like `macro`, `function`, `alias`,
`generic`, `template` etc.) in your message. They provide
a cognitive scaffolding for the user who is learning how the language
works and facilitate searching for information related to the error
in the documentation.

Pay special attention to using names conforming
with the language docs (e.g. use the word *function* but not *procedure*).

### *Speak the user's language*
Don't use jargon related to compiler internals.
If you believe that using more technical language
may be beneficial for a more experienced user, do so only after
explaining the error in simple terms.

### *Don't be so sure*
If you make a claim in the error message, make sure it is
*completely* true. Unless you're absolutely
certain about it, use modal qualifiers (like `maybe`, `might`,
`it is possible that`) to communicate to the user that there
may be an alternative explanation to the one provided in the message.

### *Here and now*
In general, messages should be written in present tense.
However, there are some exceptions:
- when referring to compiler actions (`tried to...`, `could not be found`
etc.) use past tense.

### *The compiler*
When referring to the compiler, say `the compiler` and not `duck` or some
other Duckling-specific name. You may only refer to Duckling when explaining
specific language mechanics and features.

## 2. Composition

### *Keep it short*
Keep the *header message* concise - its main role is to
inform the user about the *type* of error that has occured.
Leave more detailed explanations for other parts of
the message, most notably the *description* which is displayed *after*
the offending code fragment.

However, to keep your messages readable, avoid long sentences and walls
of text even in the *description* field.

### *Design around the code*
Diagnostics system allows for introducing short messages
inside the displayed user's code fragments.
Try to use them whenever
possible to help the user locate parts of the error
context in their own code - code-centric diagnostics
may reduce congitive load.
Remember, though, that these messages need to be
short enough for the code fragment to remain readable
and recognisable by the user.

### *Don't repeat yourself*
Try to minimise the number of times a piece of information
is repeated in the entire message. Use textual context
to your advantage.

> #### Working example
>
> Consider the following two messages:
>
> ~~~~~rs
> error[E5555]: use of a moved value `my_val`
> main.dm:15:9
>    |
>  7 |   let result = f(my_val);
>    |                  ------ value of `my_val` moved here
>    |   ...
> 15 |   x = my_val.size();
>    |       ^^^^^^ value of `my_val` used here after move
>    |
>
>
> error[E5555]: use of a moved value `my_val`
> main.dm:15:9
>    |
>  7 |   let result = f(my_val);
>                       ------ value moved here
>    |   ...
> 15 |   x = my_val.size();
>    |       ^^^^^^ value used here after move
>    |
>
> ~~~~~
> The latter is preferred as it reduces repetition of `my_val`
thanks to the use of context - there was only
one *value* mentioned in the error message, therefore
it is clear which one we're referring to.

### *Explain your reasoning*
Make sure the user understands *why* the described situation
produces an error. Oftentimes erroneous code stems from
a misunderstanding of language semantics. Use *description*
fields and additional *note* and *docs* infos to enhance your diagnostics
with reasoning and walkthroughs of how the language works.

### *Don't oversimplify the system*
If you believe your error message could be made more informative
in some special case, then consider splitting it into two
distinct versions of the message (or meticulously parametrizing
the existing one; either way, embrace specificity over generality
whenever you can afford it).

### *Use the right infos*
Remember that there are several types of
infos and each one has its own
unique goal:
- `error` - general error description,
there is only one such segment in a message
regarding one error,
- `warning` - same as error, but for emmitted
warnings,
- `note` - additional piece of information
regarding the error context,
- `hint` - suggestion of a specific change
in the user's code,
- `docs` - a short syntax or semantics explainer
with a documentation link.

## 3. Technicalities

### *error[E1001]: in lowercase*
All *header messages* and *pointer messages*
must be written in lowercase and have no punctuation at the end
(in special cases they can end with a question mark).

That is **not** the case with *descriptions*, however.
They can be multi-sentence, and thus should follow
standard punctuation rules.

### *Differentiate between code and text*
In order to distinguish between text and code inside a message, put
all quoted code in backticks.

### *Do not contract*
Avoid contractions (like *don't*, *can't*) when possible. Using a quote
in text may distract the user as it is a special character in Duckling.
