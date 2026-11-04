# Lints and warnings system in QuackPack

## Warnings

Warnings are created when a manifest is parsed.

They contain details which might be essential to the user, and are easy to check when parsing, but might require unproportional effort to check later.

Right now, they consist of:
* unused fields in a manifest,
* git repository's URLs which are also paths pointing to existing files.

## Lints

Lints guide users to avoid some patterns.

Those patterns are defined through [passes](passes.rs), for every lint there is a pass which finds whether that specific bad pattern occurs or not.
If it occurs and the pass is run, the information about it is added to [`LintBuffer`](buffer.rs).

For example, there can be a lint to avoid empty arrays in conditions, as it will evaluate to false.
However, when its pass is run, it can emit different diagnostics for dependency's or feature's conditions.

## Difference between warnings and lints

Warnings should be either simple things, or they should be easy to check while parsing a manifest.
Lints however are more sophisticated.
One example can be checking for conditions, which are always false, f.e. because they are declared with an empty array of required features.
