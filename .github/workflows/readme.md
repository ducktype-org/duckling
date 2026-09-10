# Github Workflows

## About

Directory containing "GitHub actions" files, that are scripts defining so called "workflows", that run on GitHub on given triggers.

Current workflow files:

* `linting.yml` -- defines a linting workflow, that is run on each PR
* `tests.yml`   -- defines a test workflow, that is run on each PR and the main branch

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
   In particular we are using [opened, synchronize, reopened, ready_for_review],
   so it also runs when someone un-drafts the PR.
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
> In `tests.yml` we have one job called `build`.

## Jobs language details

### Strategy

`strategy` property defines some high-level aspects of the job, in particular:

* `matrix` -- defies different configurations of the job (all configurations will run), see: <https://docs.github.com/en/actions/using-jobs/using-a-matrix-for-your-jobs>
* `fail-fast` -- defines whether if one configuration fail will other be canceled

### Runs-on

`runs-on` property defines image of OS the job will run on.

#### The build matrix and the macOS runner

`setup-build-matrix.py` decides which configurations `tests.yml`'s `build` job runs. It returns a
matrix spelled out as an `include` list rather than a `build-type` x `compiler` product, because
the configurations are not a product: macOS is one of them, not all of them.

* **unapproved PR** -- `Dev` / gcc only.
* **`main`, `dev`, `workflow_dispatch`, an approved PR, or the `Run All Workflows` label** -- four
  jobs: `Dev`/gcc (the coverage job), `DevOpt`/gcc, `DevOpt`/clang, and `Dev`/clang on macOS.
  Approval adds the label through `add-run-all-workflows-label.yml`, whose `labeled` event is what
  actually re-triggers `tests.yml`.

The macOS job **replaced** Linux `Dev`/clang instead of being added beside it, so the full matrix
is still four jobs. Nothing goes unchecked: clang at `-O3` with libstdc++ and mold stays in
`DevOpt`/clang, gcc keeps both build types, and coverage is untouched. What the macOS job adds is a
second standard library (libc++), the Apple arm64 ABI, `ld64` and a far newer clang.

Three things about that runner are worth knowing before editing those steps.

**Its accounts have no `sudo`**, so a workflow cannot install anything there. The toolchain is
whatever is already on the machine -- Homebrew's `llvm@23` to compile, `llvm@19` to link against,
ICU, cmake, ninja, ccache, plus a per-account rustup in `~/.cargo` -- which is why every
`apt`/`pip` step in `build` is guarded with `if: ${{ env.IS_MACOS != 'true' }}`. A new build
dependency has to be installed on the machine by hand first.

**Its caches never travel through `actions/cache`.** On the Linux runners the ccache and sccache
directories are uploaded and downloaded per run, and old entries have to be pruned with `gh` so
they do not snowball. On the mac there is only one machine, it is not containerised, and its disk
persists between jobs -- so the caches simply live there, under `/Users/Shared/duckling-ci`, and
every cache step in `build` is Linux-only. Two consequences:

* ccache is **shared** by `runner1` and `runner2`. Concurrent access is what ccache is built for,
  so nothing has to be synchronised between the accounts, but three non-default settings are what
  make the sharing actually work: `CCACHE_UMASK=000`, without which each account's cache files land
  at `0644` and the other account can neither reuse nor evict them, and `CCACHE_BASEDIR` +
  `CCACHE_NOHASHDIR`, without which the two accounts' differing checkout paths
  (`/Users/runner1/actions-runner/_work/...` vs `runner2`) go into the hash and the two runners
  share a directory while never taking a hit from each other. The cache is reached through a
  symlink, because `main_cmake_files/ConfigureCCache.cmake` hardcodes `cache_dir=<dev>/.ccache`
  into the compiler launcher and that overrides `$CCACHE_DIR`.
* sccache gets a directory **per account**. Unlike ccache it is not a passive directory: each
  account runs its own sccache server with its own in-memory LRU index, and two servers evicting
  from one directory corrupt each other's accounting.
* Nothing in a workflow run expires either of them, so a weekly `launchd` job on the machine
  (`org.ducktype.duckling-ci.prune`) evicts ccache entries older than two weeks and enforces the
  size cap. Note that the cap the CLI sees comes from `$CCACHE_MAXSIZE`, not from the
  `max_size=1` that `ConfigureCCache.cmake` puts on the compiler launcher - the two have to be
  kept in step by hand.

**Rust comes from rustup, not Homebrew.** `integration_tests/duck/testconfig.yaml` sources
`${CARGO_HOME:-$HOME/.cargo}/env` before `cargo build`, and that file is written by rustup only --
a `brew install rust` would leave the `duck` integration tests failing on a missing `env`.

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
* `actions/cache@v4` - an action responsible for storing files between workflow runs.
  We use it to store ccache cached build data, to speed up build times.
  Cache restoration can hit (by finding a matching cache), or miss.
  If cache is restored by exactly matching it's key, then no cache will be uploaded in spite of making changes. This is why in `tests.yml` we generate a unique key each time.

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
* `actions/github-script@v7` - action that allows to write GitHub api calls in JS.
