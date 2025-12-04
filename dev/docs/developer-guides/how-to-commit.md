# How to commit

This page describes how to commit changes to our repositories following good practices and standards. Keep in mind that this tutorial is focused on `duckling` - our repository. Other repositories do not require as strict practices.


## Repository setup

Set up your repo in a standard way. Some repositories have a `toolbox.py` script or setup instructions to facilitate the process.


## Create a branch

Most of our repositories don't allow committing directly to the `main` branch. To make any changes you need to create a new feature branch. You can do that via GH issue or link a branch to an existing issue or kanban card (see ...).


## Make some changes

Don't forget to write tests and docs! On `duckling` repo Quacker bot will block the merge if coverage percentage drops (this can be bypassed if really needed).


### Format your code

Before committing changes, make sure that your code is properly formatted.
To do that you can use our bash script. From the `dev` directory run:

```bash
./scripts/formatting/format_repo_cpp.sh
```


## Create pull request

GH will automatically run tests, check coverage and do some other things to ensure quality. You also need to get at least one positive review (you can spam `review-ping` chanel on discord or yell at people at a weekly meeting to get your review faster). Make corrections until your change is accepted and passes all tests.


## Merge changes

Use `squash and merge`. Try to provide meaningful title and description.


## Naming commits

Because we use `squash and merge` commit names on a branch can be meaningless but we require you use following naming style for commits on the main branch.

### First line of commit massage (i.e. commit tittle)

The first line of commit massage (i.e. commit tittle) consist of 5 components:

- area of changes,
- optional feature that the change works towards,
- character of changes,
- very short description, 
- issue number.

An example commit title looks as follows:

```
[Compiler] (Ft. Packages) Refactor: store root ModuleID in global Packages (#1606)
Y^^^^^^^^^ Y~~~~~~~~~~~~~ Y^^^^^^^  Y~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  Y^^^^
|          |              |         very short descriptions                 issue number
|          |              character of changes
|          Optional feature
Area of changes
```

Details of each component are described bellow.

#### Area of changes

We specify following ares (mostly relevant on the Duckling repo):

* `[GC]` -- for all GC work,
* `[Compiler]` -- for work focusing on duckc development,
* `[DVM]` -- for work focusing on DVM development,
* `[QuackPack]` -- for work focusing on QuackPack development,
* `[Base]` -- for work focusing on base module development,
* `[module-from-common]` -- for work focusing on development of given common module,
* `[Docs]` -- for work focusing on docs,
* `[Tests]` -- for work focusing on tests,
* `[DevOps]` -- for work focusing on workflows/automated quality assurance/github changes/some other automation.

If non of the above fits, you can make you own category. In such case avoid ambiguity.
If multiple fits, and there is no single that would describe the main focus, you can put multiple categories like so: `[Compiler, Logger]`.

#### Optional Feature

As the name suggest, this component should be present only if the change works towards or introduces new feature.
We don't have a specific list of features, you should name them in a simple, unambiguous way.
If possible try to use the same name that was already used before.

#### Character of changes

Character of changes describe the type of changes from the source-code point of view.
We specify following types:

- `Add`- addition of logic, docs, tests, etc,
- `Refactor` - refactor of code,
- `Fix` - fix of a bug/regression,
- `HotFix` - hot fix of a bug/regression or a quick patch added before proper change is introduces,
- `Delete`- deletion of some code/logic,
- `Update` - update of code or other stuff due to time passing (e.g. switch to new dependency version, migration, etc)
- `Maintenance` - maintenance changes that don't fit any of the above
- `Change` - some changes of code that don't fit any of the above,

If multiple commit types fit your commit, pick the best ones, and separate them with coma like so:
`Add, Refactor`.

#### Very short description

Just a very short description. Try to avoid ambiguity.
You can treat the `Character of changes` component as a verb preceding this description, but do it only if it will be clear to the reader and the description doesn't list multiple things.

#### PR number

Every change should be made through a PR. Its number should be placed in parenthesis at the end of the commit title like so: `(#1234)`, where 1234 is the PR number.

Note: GH will automatically add it to the end of the commit message when merging using the PR web ui.


### Other lines of commit massage (i.e. commit description)

We don't require any concrete style of the commit descriptions, but it is strongly advised to include in it the following:

* Short summary of all the changes.
* Motivation behind the changes.
* Any non-trivial context behind the changes.
* Example usage (if applicable).
* List of potential follow-ups.
* Any other things worth mentioning related to the changes.


## Other repositories

- On `dev-space` you can commit directly to `main`. This repository serves purely organizational purposes so quality of commits isn't that important.

- `website` repository have two branches with direct commits blocked - `main` and `dev`. All changes in this repository should be done on a separate feature branch, then merged into `dev` and then `dev` can be merged into `main`.
