# Duckling integration test framework

This README is a guide to Duckling integration test (DIT) framework.

## Test folder structure

DIT tests are composed in a tree-like structure.
At the root of `tests/` directory a top-level `testconfig.yaml` file is found.
Any tests have to be written either directly inside this file, or this
file has to link to other directories with `testconfig.yaml`.
Other directories can be listed as follows:

```yaml
Subtests:
    - C++
    - bash
```

This means that directories `C++/` and `bash/` are now considered a part of DIT test tree.
These folders have to contain `testconfig.yaml` at their roots.

## Basic variables

Variables are the core building block of DIT framework.
They are **inherited down the tree**, meaning if we create a variable in

`a/testconfig.yaml`:

```yaml
our_variable: 5
```

and `a/` has a subtests directory `b/`, then we can use a variable from up-tree as follows

`a/b/testconfig.yaml`:

```yaml
another_variable: "Our variable has value: @{our_variable}"
```

The above will evaluate to: `"Our variable has value: 5"`.


## Writing tests

In order to write a test we have to specify a few keys. Let's assume `a/testconfig.yaml` is in
the test tree. We can write a **test set** as follows

`a/testconfig.yaml`:

```yaml
Tests:
  test-shell-cat:
    Run: "cat"
    Cases:
      hello-world:
        Input:
          string: "Hello, world!"
        Output:
          string: "Hello, world!"
```

The code above when interpreted would execute a shell program `cat`.

A test named `test-shell-cat` is specified inside `Tests`. It sets the `Run` variable and lists
`hello-world` inside `Cases`. This means there only one case to run inside this test. Case is called `hello-world` and has `Input` and `Output` objects, that specify case's stdin and expected stdout respectively.

The above functionality allows us to specify multiple cases and test a program in multiple different scenarios at once.

---

Let's take a look at another example:

```yaml
Run: "@{user_variable_here}"
Tests:
  test-shell-cat:
    user_variable_here: "cat"
    Cases:
      hello-world:
        Input:
          string: "Hello, world!"
        Output:
          string: "Hello, world!"
  test-shell-echo:
    user_variable_here: "echo"
    Cases:
      hello-world:
        RunArgs: '"Hello, world!"'
        Output:
          string: "Hello, world!"
```

Everything works well, because **variables are lazily evaluated.**
The value of a variable is evaluated at the last possible moment. This does not mean however, that all the variables are evaluated at the same time, nor that `Run` can be modified inside a test.
Notice a little detail, we have given `RunArgs` to our `test-shell-echo`, because echo reads
its data from arguments instead of stdin.

## Builtin variables

All **builtin variables are `UpperCase`** and all user defined variables should
be `kebab-case` or `lower_case`.

Here is a list of builtin variables and their meaning **depending on the context**:

Meta variables:

- `Name` - Explicit name of a test set
- `Description` - Description of a test set
- `Env` - NOT IMPLEMENTED YET
- `Tests` - A dict with test set
- `Subtests` - A dict with sub tests of a config file - other nodes in the test tree.

Global variables:

- `Compile` - Command executed before running a test. Executed once per test.
- `Run` - Required - Command executed in order to run a test case.
- `Clean` - Command executed explicitly by the user to clean all the test artifacts.
- `PostRun` - Command executed after running a test case.
- `TimeOut` - Maximum time given for the execution in seconds - defaults to 1 - On timeout the process exits with exit code 124.
- `ExitCode` - Expected exit code - defaults to 0.

Test specific:

- `Name` - Explicit name of a test.
- `Description` - Description of a test.
- `Cases` - Dict with test's cases.
- Additionally: `Compile`, `Run`, `Clean`, `PostRun`, `TimeOut`, `ExitCode` as above.

Case specific:

- `Name` - Explicit name of a case.
- `Input` - Stdin passed to a program.
- `Output` - Expected stdout of a program.
- `Err` - Expected stderr of a program.
- `RunArgs` - Arguments passed to a test case
- Additionally: `TimeOut`, `ExitCode` as above.

`Input`, `Output`, `Err` inside a case can be specified as follows:

- `file` - data is taken from a given file path. DIT paths are always **relative to the current test set's config file**.
- `string` - data is a literal
- `run` - data is taken from stdout of a command specified here. Optional `compile` can be specified to first compile a program to run.

## Note on cleaning

Cleaning is **NOT** performed automatically after a test run. It is meant to be ran explicitly by the tester.

## High level notes

This system is very flexible, however it's not a build system!

Some advice I can give related to working with build artifacts includes:

- Use a build system!
- If it's an overkill then when producing a single binary file, make its suffix `.bin`.
- Otherwise, when producing multiple artifacts for a single binary, make a build command that writes everything to a */build/* directory.
