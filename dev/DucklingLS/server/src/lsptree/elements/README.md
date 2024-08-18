# Elements class structure

This class structure is an exact copy of PST elements.
It is used to implement syntax highlighting, code completion, and folding ranges.

Root class of the structure is `DucklingElement`.

## Adding new classes

When adding a new class, one has to:
- Create a class with the correct parent.
- Create a factory for the new class and connect it to the parent's factory.
- Register the new class in the new factory.
- Add the new class to `index.js`. (Factories are linked at runtime, during the first import of the `index.js` file.)