==============
Type interface
==============

Interface describes interface provided by the type, ie. its attributes and methods. In type system implementation interface is represented by TypeInterface class.

.. .doxygenclass:: ts::TypeInterface
    :members:
    :protected-members:
    :private-members:
    :undoc-members:

In practice TypeInterface just holds a map witch associates InterfaceElements with their names.

Interface element
=================

.. .doxygenclass:: ts::InterfaceElement
    :members:
    :protected-members:
    :private-members:
    :undoc-members:

.. doxygenenum:: ts::Visibility
