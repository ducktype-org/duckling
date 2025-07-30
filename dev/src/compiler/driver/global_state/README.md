This module holds a part of global state of the compiler that is not associated with any specific module.
The state is set by the driver.
The reason that this is separate module then the driver is to not require everything that uses driver to link to it.

Some parts of the global state will be represented by the query inputs, while other parts such as location of the artifacts is accessible directly.
