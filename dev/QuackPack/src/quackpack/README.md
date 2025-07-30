# Short description of top-level files

## `signals.py`

Our implementation of signal handling, allows disabling signals completely or with a robust handler.
The latter turns signals into Pythons exception raised on the main thread, which allows to run context managers cleanup.

## `package_loader.py`

Whole responsibility of this class is walking up the path looking for a manifest file.
If it finds one, new instance of `Package` class is returned.

## `package.py`

Our wrapper around package/project/venv, whatever it's called.
It keeps manifest and venv config in machine-friendly format, and also some paths.
It allows locking, to perform concurrent operations.

## `global_context.py`

This is global context/state passed throughout whole project.
