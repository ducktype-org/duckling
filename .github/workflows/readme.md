# Github Workflows

## About

Here are scripts, that are purposefully designed to automate certain actions on given triggers. Each script defines a workflow.

We are using workflows to run linter, as well as build and test our code in multiple ways.

## Syntax

Workflow file starts with a `name`, which will identify it during a CI run.

Then we specify `on`, which tells github when to run the action. In `tests.yml`, it happens
on every push and pull request to `main` or `dev` branch. There is also a `workflow_dispatch:` trigger, that just means it is possible to run the action on demand. Without, it wouldn't be possible.

`env` is used to specify some variables, that are easily accessible in a given file.

Workflows are composed of jobs, which can depend on one another. In `tests.yml` we have only one job called `build`. Each job is containerized, and sharing data between jobs in not trivial, which is the reason why building and testing is not split into multiple jobs.
Each job needs an image to run on, specified in `runs-on` and `steps`, that is a list of commands. `build` also has specified a `strategy`, which can provide a matrix and a few other parameters for parametrized jobs.

`matrix` allows us to run a job for each configuration of specified lists of parameters.
`build` has only one list of parameters - `build-type`, so there will be 2 runs - one for each element on the list, but generally there will be exponentially many runs. We can provide an arbitrary number of named lists.
`fail-fast: false` in matrix terms simply means: don't stop the workflow even if one (or more) matrix run fails. In `build` terms, e.g. we want to know a percent coverage despite `Release` failing.
`timeout-minutes` specifies maximum time before exiting the job **per matrix run**. For `build` it means, that each build can take at most `timeout-minutes` minutes to build and execute.

## Actions and job steps

Steps are the meat of a job and execute bash commands. Steps can be named, so they are easily identified during the run, and they can produce an output. Sometimes we want to store
an output of a command in a variable, and this can be achieved with:

```yaml
- name: Set a variable
  id: set-var # this is optional, but allows us to easily identify a step
  run: echo "var_name=123" > $GITHUB_OUTPUT

- name: Read a variable
  run: echo "var_name is equal to ${{ steps.set-var.outputs.var_name }}"
```

This syntax is willingly used in the `tests.yml` file.

### Workflow expressions

Accessing a variable achieved with `${{ ... }}` - Github workflow expression, which are powerful enough to e.g. compare values, or even make a ternary operator: `${{ <EXPR> && <VALUE_ON_TRUE> || <VALUE_ON_FALSE> }}`. Variables support multiple data types. String values evaluate to `true`, when they are non-empty, which is also used multiple times throughout `tests.yml` file. Expressions are actually very well documented [here](https://docs.github.com/en/actions/learn-github-actions/expressions).

### Actions

Github has a marketplace for actions, which are open-source programs (scripts) specifically
designed to make certain tasks easier.

* `actions/checkout@v4` - puts a repository into the runner.
* `actions/cache@v4` - an action responsible for storing files between workflow runs.
    It is especially useful for us, since it allows us to cut down build times from 6 minutes to under 1 minute. Cache restoration can hit (by finding a matching cache), or miss.
    If cache is restored by exactly matching it's key, then no cache will be uploaded in spite of making changes. This is why we generate a unique key each time.
    Parameters:
  * `path` - required - a list of directories to store in a cache file.
  * `key` - required - a key which uniquely identifies a cache. Cache files cannot be overridden, which means that if we want to have incremental builds in `tests.yml`, we need to generate a new key every time there has been a change in the build folder, which
    still holds if we generate a new key for each run of the workflow. This way it doesn't make us run a complicated hashing function for a each build.
  * `restore-keys` - optional - a list of keys, that are fuzzily matched top to bottom with available caches. If an entry matches multiple caches' names, then the most recent cache is used. Choosing the most recent file has nothing to do with the date in the cache file, but merely it's file modification date. It just so happens that, in our case, date in the filename and the date in the file properties are almost identical.
