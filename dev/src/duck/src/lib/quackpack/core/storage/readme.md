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

In the second variant, when the `quackfreeze.json` file is provided in the package directory,
any synchronization operation will first try to use it as the realization
of manifest's dependencies. If it is invalid or not present, the solver will be called instead,
and its result will be exported to the freezefile in the package directory.
Note that the freezes are also present in the storage venv state for the purposes
of the clean operations — they still pin their used packages in the storage.
