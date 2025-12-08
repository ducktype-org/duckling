<div align="center">
    <picture>
        <source media="(prefers-color-scheme: dark)" srcset="images/readme_logo_yellow.svg">
        <source media="(prefers-color-scheme: light)" srcset="images/readme_logo_dark.svg">
        <img alt="Duckling programming language" src="images/readme_logo_dark.svg" width="50%">
    </picture>
    <!-- <p>Duckling is the in-development programming language</p> -->
</div>

-----

<p align="center">
  <a href="https://docs.duckling.pl/">Documentation</a>
  &nbsp;·&nbsp;
  <a href="https://duckling.pl/">Website</a>
  &nbsp;·&nbsp;
  <a href="https://duckling.pl/get_involved/">Community</a>
</p>

-----


**This is the main Duckling development repository of the Duckling programming language** – a modern, in-development programming language focusing on bridging scripting and compiled worlds together, providing fast prototyping and low friction while maintaining scalability and performance.

# Important Notice

Duckling is an in-development programming language.
We encourage you to try it out, provide feedback and get involved,
but at this point you should still expect the compiler and toolchain to not be stable,
and breaking changes to be introduced frequently.

# Project Status

Duckling is an in-development programming language. The first ideas for the language appeared in 2021, and proper development started in 2023.
Our focus and development status as of December 2025, divided by the main sub-parts of the project is as follows.

* **Documentation** – We have stabilized initial documentation of the language, covering most important features and setting a vision for a 1.0 version of the language. A lot of work still has to be done, but we are not focusing on the docs right now. Instead we are prioritizing the implementation of the compiler and other tools.

* **Compiler** – Duckling compiler is currently our main priority. We have done most of the architectural groundwork and now focusing on language features. There are still few features missing to be able to truly program in Duckling, but at the same time we have already implemented some of the features that make Duckling stand out such as fine-grained incremental compilation or compile time evaluation.

* **Virtual Machine** – Duckling Virtual Machine (DVM) is being developed alongside compiler and is central to a large part of Duckling goals. As of today, Ducklings compiler can emit its bytecode and DVM already serves as a compile-time execution engine, but there are a lot of core functionalities yet to be developed. Currently we are focusing on development of large part of such features including for example JIT compilation or fully safe execution. For the time being LLVM will remain the primary backend of the compiler.

* **Language Server** – Ducklings language server underwent a lot of fundamental changes in the past, but now we have landed on the final architecture. The architecture is based almost entirely on the stateful-ness of the Ducklings compiler and as a result has potential to streamline future development and make the server extremely performant on large codebases. Right now we are focusing on finalizing the said architecture and after that we will shift focus to developing and stabilizing most important features. In terms of editor support, we have working VSCode extension that uses the language server and we will likely not focus on other editors until the core aspects of the language server are set in stone.

* **QuackPack** – QuackPack is a Ducklings package manager. Its still in its very early stage, and we are currently in the process of rewriting and reshaping it. Our current priority is to create minimal working solution that seamlessly integrates with other tooling. Together with QuackPack we are also developing a relatively small tool called `duck`, which serves as a main CLI interface of the entire ecosystem.

# Why Duckling?

The project focuses on creating a language that scales seamlessly — from small scripts to large projects. We want the language to provide a comfortable scripting and prototyping experience, and to remain frictionless as the project grows. This introduces a set of seemingly contradicting goals. For example, the language has to be compiled to machine code, but at the same time it must be usable in notebooks (like Jupyter). We tackle such challenges not only by proper language and toolset design, but also through robust technological improvements that unlock new possibilities currently beyond the reach of most mainstream languages. This includes:

* **Stateful compilation**. The `duckc` compiler is not built around a traditional paradigm of black-box, pass-based compilation in which each compilation process performs single, atomic operation. Rather it is a stateful, query-based compiler that can store and modify live state within memory and on disk. This opens up essential qualities that enable features such as fine-grained incremental compilation, gradual compilation in REPL environment or acting directly as a language server. 

  <!-- We believe that the quality of the compiler is one of the most important aspects of a programming language, and can often have a bigger impact on the language than the language’s design. The compiler impacts areas such as compilation speed, error messages, tooling integration, and even subtler aspects like how quickly the language can evolve.  -->

* **CPU-DVM dual architecture**. Duckling compilation targets two backend – dedicated Duckling Virtual Machine and LLVM IR. The former acting as a primary development environment, compile time evaluation engine, debugger, script execution engine while the latter enables Duckling to be compiled to highly performant code on a wide range of architectures.

You can read more about Duckling technology and goals in the [Documentation](https://docs.duckling.pl/duckling/introduction/index.html) and on the [Duckling website](https://duckling.pl/pl/why_duckling/).

# Documentation website

User documentation of the language and related tools can be found at [https://docs.duckling.pl/](https://docs.duckling.pl/).

