# Documentation guidelines

@TODO:
Write more tutorials

This is an overview of all documentation in the Duckling project.


## Documentation structure

Duckling documentation is divided into three parts:

- [duckling-doc](#duckling-documentation) - user documentation, describing language funcionalities, usage, assumptions and goals 
- [source-doc](#source-documentation) - developer documentation, guidelines and resources for developers
- [doxygen](#doxygen-documentation) - low level description of source code, implementation details and libraries usage

[source-doc](#source-documentation) is further divided into:

- [dev-guides](../../dev-guides/index.md) - guidelines, tutorial and instructions
- [dev-handbook](../../dev-handbook/index.md) - high level description of source code


## Duckling documentation

This is the user documentation, describing language funcionalities, usage, assumptions and goals.
You can find it in the [rift-doc](https://github.com/ducktype-org/rift-doc) github repository and is currently under development.


## Source documentation

You are currently in the source documentation. This part of the documentation is intended for developers contributing to Duckling.
It is in the main Duckling repository in the `docs` directory.

It is generated using Sphinx and reStructuredText (rst) format, for which you can find a quickstart guide here: [rst-quickstart](rst-quickstart.md).


## Doxygen documentation

Doxygen documentation is generated from source code comments. 
It is intended for developers who want to understand the implementation details of Duckling.

A quick overview of Doxygen documentation can be found here: [doxygen-quickstart](doxygen-quickstart.md). 

You can also write Markdown documentation that will be included in Doxygen, see [markdown-quickstart](markdown-quickstart.md).


### Excluded directories

The following directories are excluded from Doxygen documentation (you can find them in the `Doxyfile.in` file):

- `*build*/*`
- `docs/*`
- `*/examples/*`
- `*/tests/*`


## See also

- Printer examplary documentation (now inside Doxygen, search for `printer`)

- [Discord documentation channel](https://discord.com/channels/860531247826731029/1106545291852783627)
