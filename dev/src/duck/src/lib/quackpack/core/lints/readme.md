# Lints and warnings system in QuackPack

## Warnings

Warnings are created when a manifest is parsed.

They contain details which might be essential to the user, and are easy to check when parsing, but might require unproportional effort to check later.

Right now, they consist of:
* unused fields in a manifest,
* git repository's URLs which are also paths pointing to existing files.

## Lints

Lints guide users to avoid some patterns.

They are run through [passes](passes.rs), and can be registered there, while [`LintContext`](context.rs)
takes care of emitting them.

For example, there can be a lint to avoid empty arrays in conditions, as it will evaluate to false.
However, when its pass is run, it can emit different diagnostics for dependency's or feature's conditions.
