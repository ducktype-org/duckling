# Basic idea

Documentation should mainly provide two things:

* Clearly show how and when to use the module, and what exactly can it do.
* Provide an entry point for anyone trying to create a change or fix an error in the module.

It is important to note that the second task should be done mainly with 
source code level comments, and documentation files should, as far as possibly, only provide
top level view of module implementation.

# Format

Documentation is usuals single `.md` file that consist of 
description, interface, examples and additional details.
[DocumentationTemplate](DocumentationTemplate.md) is file that represent the structure
and basic rules that typical documentation should follow.

All additional files should be visibly referenced in main one by relative links 
in proper sections.

Additional files might consist of, but are not limited to:
code/usage examples, more detailed descriptions. It is advised to put larger code examples in
separate files.

Documentation can also link module test as code examples.

# Comments vs documentation.

Unless it is justified documentation shouldn't go into details about
source code beyond things listed in the template.
Such information should be given inside source code comments.

# Last note

In special cases guidelines can be broken within reason.
It is however unadvised, as any irregularities will make it more difficult
to understand.

