# Duckling Github repositories explained

Here you can find description of all  active repositories in our project.


## `duckling`

The main repository of the project. Source code is located in the `dev/` directory. Setup is automated with `toolbox.py`.


### Source code structure

- `DucklingLS` - duckling language server code
- `compiler` - compiler code
- `VM` - virtual machine code
- `base` - custom module with standard-library-like implmentations
- `common` - commonly used modules
- `docs` - documentation


## `dev-space`

This repository is a place to organize our workflow and keep materials that are not directly connected to the code.


### Repository Structure

Here you can find description of the most important parts of the repository.

- `organizacja/`
    - `dev/`
        - `Q<n>/` - Summary of the n-th quarter of the project
        - `plan.md` - general developement schedule
        - `scenariusz_weekly.md` - weekly dev team meeting checklist
    - `misje/` - mission files
        - `aktywne/` - current missions
        - `wstrzymane/` - frozen missions
        - `zakonczone/` - finished mission
    - `podsumowanie-dyskusji/` - discussion notes
    - `propozycje_misji/` - mission suggestions
    - `raport_gc/` - ground control reports
    - `website/` - website notes and reports
    - `zpp/` - zpp notes and reports
    - `organizacja.md` - workflow description
    - `osoby.md` - contact info to (almost) all team members
- `prace_naukowe/` - zpp theses and other articles

Most information about current and finished tasks can be found in `misje/` directory 
(if you don't know what missions are, check out [work-organization](work-organization.md) and [dev-space](https://github.com/ducktype-org/dev-space/blob/main/organizacja/orgranizacja.md)). 
Each mission directory has a `info.md` file with general mission description, 
`raporty/` directory with progress reports and other files with notes and materials. 
Finished missions also have final report with mission summary.


## `rift-doc`

This is the main duckling documentation repository, written for future duckling users. It describes how to write duckling code and how it works.
For instructions on how to build the documentation, check out [main README.md](https://github.com/ducktype-org/rift-doc).


## `website`

In this repository you can find all of our website files. `main` branch represents current public version of the website (available at [ducktype.org](https://ducktype.org/) and [duckling.pl](https://duckling.pl/)) and `dev` branch represents current stable development version (available at [testsite.ducktype.org](https://testsite.ducktype.org/)).

All instructions on how to build and deploy the website can be found in the `dev-space` repository in the `website/` directory, 
link [here](https://github.com/ducktype-org/dev-space/tree/main/organizacja/website/how-to).


## `doc-confic`

This repository is dedicated to configuration of all of our sphinx-based documentation. It is included as a submodule in `rift-doc` and `duckling`.


## `zpp`

Each year some zpp teams join our organization. Since their projects are usually somewhat independent from core language developement, they work on a separete repository (usually fork of `duckling`) and their work is later synced-up or merged into core repos. These repos are marked with `-zpp` suffix and are poject specific.
