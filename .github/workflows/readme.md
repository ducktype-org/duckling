# Github Workflows

## About

Directory containing "GitHub actions" files, that are scripts defining so called "workflows", that run on GitHub on given triggers.

Current workflow files:

* `linting.yml` -- defines a linting workflow, that is run on each PR
* `tests.yml`   -- defines a test workflow, that is run on each PR and the main branch
* `rust.yml`    -- clippy, fmt and test for the Rust crates
* `docs.yml`    -- builds the doxygen documentation, on `main` only
* `quacker.yml` -- entry point for quacker-bot, handling all four of its triggers
* `determine-runners.yml` -- reusable workflow choosing between a self-hosted runner and a
  GitHub-hosted one, called by the others through `uses:`
* `add-run-all-workflows-label.yml` -- puts the `Run All Workflows` label on an approved PR, which
  is what re-triggers `tests.yml` with the full matrix
* `copilot-setup-steps.yml` -- the environment GitHub Copilot's agent gets

Two helpers live here as well, next to the workflows that call them: `setup-build-matrix.py` and
`prune-caches.py`.

Workflow always runs on some commit, and is then linked to it.

## Workflow language

Since "language" used to define workflows is not something we work with every day, here is a quick summary of its most important concepts and some unintuitive behaviors.

### Workflow name

`name` property at the beginning of the file gives workflow its name.

> [!NOTE]
> Branch rulesets ignore this name, and only take into account the job names.

### Workflow dispatch triggers
`on` property defines events that trigger workflow runs.

Notable triggers:
* `push` -- on push to specified branches
* `pull_request` -- on some pull request activities.
   The default ones are opened, synchronize, reopened.
   We can change them by adding additional property `types: [...]`.
   In particular we are using [opened, synchronize, reopened, ready_for_review, labeled],
   so it also runs when someone un-drafts the PR, and when the `Run All Workflows` label
   is added.
   See <https://docs.github.com/en/actions/reference/events-that-trigger-workflows#pull_request> for
   more details.
* `workflow_dispatch` -- on manual trigger

GH docs: <https://docs.github.com/en/actions/using-workflows/events-that-trigger-workflows>

### Workflow environment
`env` property specify variables, that are easily accessible in a given workflow file.

### Workflow jobs

`jobs` property specify actual list of things to do.

Jobs may depend on success of other jobs.
Each job is containerized, and sharing data between jobs in not trivial, which is the reason why building and testing is not split into multiple jobs.

> [!TIP]
> In `tests.yml` the compiling and testing all happens in one job, `build`. The three jobs beside
> it carry no build between them: `setup` picks the matrix, `prune-caches` deletes superseded
> cache entries once the matrix is done, and `code-statistics` reports.

## Jobs language details

### Strategy

`strategy` property defines some high-level aspects of the job, in particular:

* `matrix` -- defies different configurations of the job (all configurations will run), see: <https://docs.github.com/en/actions/using-jobs/using-a-matrix-for-your-jobs>
* `fail-fast` -- defines whether if one configuration fail will other be canceled

### Runs-on

`runs-on` property defines image of OS the job will run on.

#### The build matrix

`setup-build-matrix.py` decides which configurations `tests.yml`'s `build` job runs. It returns a
matrix spelled out as an `include` list rather than a `build-type` x `compiler` product

* **unapproved PR** -- `Dev` / gcc only.
* **`main`, `dev`, `workflow_dispatch`, an approved PR, or the `Run All Workflows` label** -- four
  jobs: `Dev / gcc` (the coverage job), `DevOpt / gcc`, `DevOpt / clang` and
  `Dev / clang-23-macos`. Those strings are also the status-check names the `main` ruleset
  requires -- the check name is the JOB name, so renaming a matrix entry means editing the
  ruleset
  Approval adds the label through `add-run-all-workflows-label.yml`, whose `labeled` event is what
  actually re-triggers `tests.yml`.

What the macOS job adds is a
second standard library (libc++), the Apple arm64 ABI, `ld64` and a far newer clang.

The two platforms prepare a build so differently that neither preparation is in `tests.yml` at
all. It lives in `.github/actions/`, in four composite actions, one per platform per concern.

### Job timeout

`timeout-minutes` property defines timeout in minutes per configuration run.


## Job steps

Each job is perform a list of steps. Each step consist of either a bash command to run or outside action to execute (see below).

Steps can be named, so they are easily identified during the run, and they can produce an output. Sometimes we want to store an output of a command in a variable, and this can be achieved with:

```yaml
- name: Set a variable
  id: set-var # this is optional, but allows us to easily identify a step
  run: echo "var_name=123" > $GITHUB_OUTPUT

- name: Read a variable
  run: echo "var_name is equal to ${{ steps.set-var.outputs.var_name }}"
```

This syntax is used in the `tests.yml` file.

### Workflow expressions

Accessing a variable can be achieved with `${{ ... }}` - a Github workflow expression, which are powerful enough to e.g. compare values, or make a ternary operator: `${{ <EXPR> && <VALUE_ON_TRUE> || <VALUE_ON_FALSE> }}`. Variables support multiple data types. Expressions are very well documented [here](https://docs.github.com/en/actions/learn-github-actions/expressions).

> [!IMPORTANT]
> String values evaluate to `true`, when they are non-empty, which is also used multiple times throughout `tests.yml` file.

## Outside Actions

Github has a marketplace for actions, which are open-source programs (scripts) specifically designed to make certain tasks easier.
Outside actions we use:

* `actions/checkout@v6` - puts a repository into the runner.
  If you want to do a checkout from a different repository in our organization (like submodule), 
  you have to pass aditional options and use a personal access token.  
* `actions/cache/restore@v5` and `actions/cache/save@v5` - an action responsible for storing files
  between workflow runs. We use it to store ccache and sccache cached build data, to speed up build
  times.
  Cache restoration can hit (by finding a matching cache), or miss.
  If cache is restored by exactly matching it's key, then no cache will be uploaded in spite of making changes. This is why in `tests.yml` we generate a unique key each time.

  The two halves are used separately rather than as the combined `actions/cache`, because the
  restore and the save want different conditions and different positions: the save is backgrounded,
  has to sit after the last step that writes into the directory, and on `main` the restore is
  skipped while the save is not. `rust.yml` only ever restores -- the entries it reads are the ones
  `tests.yml` saved.

  > [!IMPORTANT]
  > `main` deliberately builds from an **empty** cache. Its restore carries
  > `lookup-only: ${{ env.BRANCH_NAME == 'main' }}`, which checks that the entry exists (the save
  > step reads `cache-primary-key` from it) without downloading it. The entry `main` then saves
  > holds one fresh build rather than every object ever compiled on `main` -- and since every
  > branch falls back to that entry, its size is everyone's download time.

  Parameters:
  * `path` - required - a list of directories to store in a cache file.
  * `key` - required - a key which uniquely identifies a new cache entry generated by this workflow.
  * `restore-keys` - optional - a list of keys, that are matched top to bottom with available cache entries.
  If an entry matches multiple caches' names, then the most recent cache is used.
  Details on how the cache entry are matched: <https://docs.github.com/en/actions/using-workflows/caching-dependencies-to-speed-up-workflows#matching-a-cache-key>.

  > [!IMPORTANT]
  > Choosing the most recent cache entry has nothing to do the date component in the key that we generate.

* `action-pack/set-variable@v1` - action for storing a [repository variable](https://docs.github.com/en/actions/learn-github-actions/variables#creating-configuration-variables-for-a-repository), which is persistent between runs.
  In `tests.yml` we use it to store the percent coverage of main, so we can use it to compare to percent coverage in other pull requests.
* `actions/github-script@v9` - action that allows to write GitHub api calls in JS.
* `actions/upload-artifact@v4` - keeps quacker-bot's full output log, untruncated, for debugging a
  run that went wrong.
* `cpp-linter/cpp-linter-action@v2` - the C++ linter `linting.yml` is built around.
* `int128/hide-comment-action@v1` - hides the earlier `quacker-bot` comments on a PR against
  `main`, so that only the newest statistics comment is left expanded.
