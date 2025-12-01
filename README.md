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
  ·
  <a href="https://duckling.pl/">Website</a>
  ·
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

* **Virtual Machine** – Duckling Virtual Machine (DVM) is being developed alongside compiler and is central to a large part of Duckling goals. As of today, Ducklings compiler can emit its bytecode and it already serves as a compile-time execution engine, but there are a lot of core functionalities to be developed. Right now we are focusing on developing large part of them including for example JIT compilation or fully safe execution. 

* **Language Server** – Ducklings language server underwent in the past a lot of fundamental changes and now we landed on the final architecture, that is based almost entirely on the stateful-ness of the Ducklings compiler. Right now we are focusing on finalizing the said architecture and after that we will shift focus to developing and stabilizing most important features. In terms of editor support, we have working VSCode extension that uses the language server and we will likely not focus on other editors until the core aspects of the language server are set in stone.

* **QuackPack** – QuackPack is a Ducklings package manager. Its still in its very early stage, and we are currently in the process of rewriting and reshaping it. Our current priority is to create minimal working solution that seamlessly integrates with other tooling. Together with QuackPack we also develop a relatively small tool called `duck`, which serves as a main CLI interface of the entire ecosystem.

# Why Duckling?

...

# Documentation website

User documentation of the language and related tools can be found at [https://docs.duckling.pl/](https://docs.duckling.pl/).

