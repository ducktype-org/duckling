**NOTE**: there is a chapter about storage in the bachelor thesis.
Freezefiles however are not described there (so the main emphasis of this
file is to explain how they work).

# Overview

Virtual environments in Quackpack do not store their downloaded dependencies
in the project directory, instead all dependencies are stored in a *storage*.
There can be many storages on the system, and many virtual environments may
correspond to one storage.
User's project contains manifest which declares what dependencies the project
has and during *synchronization* operation, solver is used to obtain
a build graph satisfying those dependencies.
This complete information required to build a package is called a *freeze*,
the storage stores freezes for virtual environments together with some
metadata such as last path under which the virtual environment was seen.

There is also an option to store the freeze externaly in the user project
in the form of a freezefile (this is more useful for projects where the freezefile
needs to be checked in to a version control system; simple scripts may benefit from
less user visible footprint by using the storage-based freezes).

# Freezefile logic

There are two possible behaviours controlled by `freezefile_exposed` flag in the
virtual environment config:
1. freeze stored in the storage
2. freezefile exposed to the user

In the second variant, when a file `quackfreeze.json` is provided in the package directory,
any synchronization operation will first try to use it as the realization
of manifest dependencies. If it is invalid or not present, solver will be called instead,
and its result will be exported to the freezefile in the package directory.
Note that the freezes are also present in the storage venv state for the purposes
of clean operations --- they still pin their used packages in the storage.
