# Duckling Integration Test framework

This README is a guide to Duckling Integration Test (DIT) framework.

## Test folder structure

DIT tests are composed in a tree-like structure.
At the root of `integration_tests/` directory a top-level `testconfig.yaml` file is found.
Any tests have to be written either directly inside this file, or this
file has to link to other directories with `testconfig.yaml`.
Other directories can be listed as follows:

```yaml
SubDirs:
    - SomeC++Tests
    - bash
```

This means that subdirectories `SomeC++Tests/` and `bash/` are now considered a part of DIT test tree.
These folders have to contain `testconfig.yaml` at their roots.

## Basic variables

Variables are the core building block of DIT framework.
They are **inherited down the tree**, meaning if we create a variable in

`a/testconfig.yaml`:

```yaml
our_variable: 5
```

and `a/` has added a sub directory `b/`, then we can use a variable from up-tree as follows

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
          String: "Hello, world!"
        Output:
          String: "Hello, world!"
```

The code above when interpreted would execute a shell program `cat`.

A test named `test-shell-cat` is specified inside `Tests`. It sets the `Run` variable and lists
`hello-world` inside `Cases`. This means there is only one case to run inside this test. Case is called `hello-world` and has `Input` and `Output` objects, that specify case's stdin and expected stdout respectively.

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
          String: "Hello, world!"
        Output:
          String: "Hello, world!"
  test-shell-echo:
    Cases:
      hello-world:
        user_variable_here: 'echo "Hello, world!"'
        Output:
          String: "Hello, world!"
```

Everything works well, because **variables are lazily evaluated.**
The value of a variable is evaluated at the last possible moment. This does not mean however, that all the variables are evaluated at the same time. `Run` is evaluated per test **case**.
Notice a little detail, we have given arguments to our `test-shell-echo/hello-world`, because echo reads
its data from arguments instead of stdin.

Variables can be shadowed.

## Builtin variables

All **builtin variables are `UpperCase`** and all user defined variables should
be `kebab-case` or `lower_case`.

> [!IMPORTANT]
> The only exceptions are:
> * `@{dev_dir}`, which is an absolute path to the `dev/` directory,
> * `@{build_dir}`, which is a value of the `-b` CLI option.

Here is a list of builtin variables and their meaning **depending on the context**:

Config file variables (linked to a node in the test tree, not inherited):

- `Name` - Explicit name of a test set
- `Description` - Description of a test set
- `Tests` - A dict with test set
- `SubDirs` - A dict with sub tests of a config file - sub nodes in the test tree. These have to be direct subdirectories of a parent directory of the config file.

General variables (not tied to any context):

- `Run` - Required - Bash command executed in order to run a test case.
- `Clean` - Bash command executed explicitly by the user to clean all the test artifacts.
- `PreTest` - Bash command executed **before** running a test.
- `PostTest` - Bash command executed **after** running a test.
- `PreCase` - Bash command executed **before** running a test case.
- `PostCase` - Bash command executed **after** running a test case.
- `TimeOut` - Maximum time given for the execution in seconds - defaults to 1 - On timeout the process exits with exit code 124.
- `NeededThreads` - How many machine threads one case of this test occupies while it runs - defaults to 1. See [Concurrency](#concurrency).
- `ExitCode` - Expected test case's exit code - defaults to 0.
- `Enabled` - Bash command specifying whether the test case is enabled. If it evaluates to true (0), then the test case is enabled, otherwise it's disabled.
- `Env` - A mapping of **environment variable names to bash commands**. The commands are evaluated once per case, in definition order (later entries see the earlier ones); each command's stdout becomes the variable's value. The resulting environment is passed to every command of the case. Entries are inherited down the tree and can be shadowed per key.

  ```yaml
  Env:
      DIT_TMP_DIR: "@{new_tmp_dir}"           # a fresh mktemp dir for every case
      DUCK_HOME: "echo $DIT_TMP_DIR/duck_home"  # may derive from earlier entries
  ```

- `ConfigDir` - Absolute path to a directory containing the current `testconfig.yaml` file.
  > [!IMPORTANT]
  > Remember that variables are expanded lazily, so `SubDirs` can overwrite your `ConfigDir`.

  > [!NOTE]
  > `@{ConfigDir}` points to the parent directory of the `testconfig.yaml` file (__inside__ git).
  >
  > It's not affected by temporary directories.

Subtree-specific variables (applied to a subtree rooted at this node) are __INHERITED__ from the parent node unless explicitly redefined in the child node:
- `PreNode` - A command executed once before processing the node and its subdirectories.
  Example: If a `PreNode` command is `echo '1'` and the node has 3 subdirectories, the command will run 4 times in total: once for the parent node and once for each subdirectory.
  Order of execution: `parent node` -> `first child` -> `second child` -> `third child` -> `parent's siblings`.

- `PostNode` - Same as `PreNode`, but the command is executed after processing the node and its subdirectories.
  Order of execution: `first child` -> `second child` -> `third child` -> `parent node` -> `parent's siblings`.

Both `PreNode` and `PostNode` commands are executed only if at least one test case beneath the node matches the `-t` filter argument. This ensures that irrelevant nodes and their commands are skipped.

Test specific:

- `Name` - Explicit name of a test.
- `Description` - Description of a test.
- `Cases` - Dict with test's cases.
- Additionally, all the general variables are accepted.

Case specific:

- `Name` - Explicit name of a case.
- `Input` - Stdin passed to a program.
- `Output` - Expected stdout of a program.
- `Err` - Expected stderr of a program.
- Additionally: `Run`, `PreCase`, `PostCase`, `TimeOut`, `NeededThreads`, `ExitCode`, `Enabled` explained above.

`Input`, `Output`, `Err` inside a case can be specified as follows:

- `File` - data is taken from a given file path. DIT paths are always **relative to the current test set's config file**.
- `String` - data is a literal
- `Run` - data is taken from stdout of a command specified here.
- `Compile` - [Optional] - can be specified to first compile a program to run.

## Concurrency

Tests run concurrently by default (`-j` defaults to the CPU count; pass
`--sequential` / `-s` to run one case at a time, which is useful for debugging).
**Tests must be written so that they can run concurrently.** The scheduler
guarantees only the following order:

- `PreNode` of a node runs **before** all tests and subnodes of that node,
  and `PostNode` runs **after** all of them.
- `PreTest` runs **before** all cases of its test, `PostTest` **after** them.
- Anything in between — cases of a test, tests of a node, sibling subtrees —
  may run in **any order and in parallel**.

Practically this means a case must not write to files shared with other cases:
no artifacts in the test's source directory, no shared scratch paths. Use the
per-case temporary environment below for anything a case writes.

`-j` is a budget of machine **threads**, not a count of cases: a case declaring
`NeededThreads: N` holds `N` of them for its whole run, and the scheduler starts
another case only once the remainder covers it. Cases that spawn their own
threads or processes should declare it, otherwise `-j 22` runs 22 of them at
once and the machine ends up with several times that many runnable threads —
everything then slows down and the tight `TimeOut`s start failing:

```yaml
duckc_worker_count: "3"
NeededThreads: "@{duckc_worker_count}"   # inherited by every case below
```

It is a general variable, so it can be set on a node, a test or a single case,
and it is inherited down the tree like the rest.

Keep `NeededThreads` within the `-j` a run realistically gets: a case asking for
more than the whole budget is rejected, since it could never be scheduled.
`--sequential` runs one case at a time and ignores the budget entirely, so a
wide test set stays debuggable on a small machine.

A test can opt out of parallelism entirely with `NoParallel: true` (settable on the test or inherited
from any ancestor node): all such tests are deferred to a second phase after
every other test has finished, where their cases run **strictly one at a time
on an otherwise idle machine**. Use it sparingly, for tests sensitive to
machine load, e.g. tight timeouts. A `PostNode` above a `NoParallel` test
correctly waits for that second phase.

Every command runs with core dumps disabled (`ulimit -c 0`): a crashing case
would otherwise not exit until the system core-dump handler drains its core,
which can take seconds on a busy machine and stalls unrelated tests behind a
system-wide handler. Pass `--core-dumps` to keep cores when debugging a crash.

`TimeOut`s are wall-clock and tuned for a lightly loaded machine; on slow or
busy machines scale them all with `--timeout-scale <factor>`.

Each finished test prints its output as one atomic section, by default in
completion order; pass `--deterministic-output` to print in the definition
(tree) order instead — normal tests first, `NoParallel` tests last.

The `-t` filter is a regex, matched with `re.search` against the full
`node/.../test/case` path of every case. Plain strings therefore work as
fuzzy filters: `-t integration_tests/compiler` (a prefix), `-t pointers`
(an inner directory), or `-t 'slices.*oob'`. Anchor with `^`/`$` or
surrounding `/` for exact segments.

## Temporary environments

A subtree that needs a scratch directory opts in with:

```yaml
Env:
    DIT_TMP_DIR: "@{new_tmp_dir}"
```

Every case then gets a fresh directory under the suite's scratch root, visible
to all of its commands as `$DIT_TMP_DIR`. That root is `/tmp/dit-$(id -un)`, so
several accounts can run the suite on one machine without stepping on each
other; set `$DIT_TMP_ROOT` to put it somewhere else. The root config defines
helpers around `integration_tests/helpers/tmp_env.py`:

- `@{make_tmp_env}` - copies the files listed in the `tmp_env_files` variable
  (paths relative to the test's directory, mirrored inside the tmp dir) and
  sweeps stale directories of past runs. Not needed if nothing is copied.
- `@{at_tmp_env} CMD` - runs `CMD` inside the tmp dir.
- `@{cleanup_tmp_env}` - removes the tmp dir; put it in `PostCase`, which only
  runs for successful cases - directories of failed cases are kept in the
  scratch root for debugging and are swept once they age out.

Often no files need copying at all: point the compiler's artifact option at
`$DIT_TMP_DIR/build` (see `compiler/compilation/testconfig.yaml`) and keep
reading sources from the test's directory, which is safe because it is
read-only sharing.

## Note on cleaning

Cleaning is **NOT** performed automatically after a test run nor before. It is meant to be ran explicitly by the tester.

## High level notes

This system is very flexible, however it's not a build system!

Some advice I can give related to working with build artifacts includes:

- Write artifacts to `$DIT_TMP_DIR` (see the temporary environments section) — never to the source tree.
- Use a build system when the setup outgrows the framework.

## Troubleshooting and debugging

If your tests happen no to work as intended, flags `-v/--verbose` and `-d/--dry` are highly recommended. Dry run only displays the commands
that would be ran, instead of really executing them, while verbose runs the commands, but also prints their output where possible.
