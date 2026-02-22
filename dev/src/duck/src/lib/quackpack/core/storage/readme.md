NOTE: there is a chapter about the storage in the bachelor thesis.
Freezefiles however are not described there (so the main emphasis of this
file is to explain how they work).

# Overview

Virtual environments in QuackPack do not store their downloaded dependencies
in the project directory, instead all dependencies are stored in a *storage*.
There can be many storages on the system, and many virtual environments may
correspond to one storage.
A user's project contains a manifest which declares what dependencies the project
has. During the *synchronization* operation, the solver is used to obtain
a build graph satisfying those dependencies.
This complete information required to build a package is called a *freeze*.
The storage stores freezes for virtual environments together with some
metadata such as the last path under which the virtual environment was seen.

There is also an option to store the freeze externally in the user's project
in the form of a freezefile (this is more useful for projects where the freezefile
needs to be checked into a version control system; simple scripts may benefit from
less user visible footprint by using the storage-based freezes).

# Freezefile logic

There are two possible behaviours controlled by the `freezefile_exposed` flag in the
virtual environment config:
1. freeze stored in the storage,
2. freezefile exposed to the user.

In both cases, storage will use an old freeze as a base for a new dependency resolution.
Note that, regarding the `freezefile_exposed` flag, current freeze is *always* stored in
a virtual environment.
In other words, setting `freezefile_exposed` affects only two things: whether to consider user-exposed freeze
(`quackfreeze.json`), and whether to update it after finding new dependencies.

It's also worth mentioning that "to consider user-exposed freeze" means, that we prioritize
user-exposed freeze over storage's freeze: but if user didn't have a freezefile, we *would* use freeze stored in
storage.
