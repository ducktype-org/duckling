# Lints and warnings system in QuackPack

## Warnings

Warnings are created when a manifest is parsed.

They contain details which might be essential to the user, and are easy to check when parsing, but might require unproportional effort to check later.

Right now, they consist of:
* unused fields in a manifest,
* git repository's URLs which are also paths pointing to existing files.
