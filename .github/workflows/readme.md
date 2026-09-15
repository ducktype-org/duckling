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

#### The build matrix and the macOS runner

`setup-build-matrix.py` decides which configurations `tests.yml`'s `build` job runs. It returns a
matrix spelled out as an `include` list rather than a `build-type` x `compiler` product, because
the configurations are not a product: macOS is one of them, not all of them.

* **unapproved PR** -- `Dev` / gcc only.
* **`main`, `dev`, `workflow_dispatch`, an approved PR, or the `Run All Workflows` label** -- four
  jobs: `Dev / gcc` (the coverage job), `DevOpt / gcc`, `DevOpt / clang` and
  `Dev / clang-23-macos`. Those strings are also the status-check names the `main` ruleset
  requires -- the check name is the JOB name, so renaming a matrix entry means editing the
  ruleset in the same breath, or PRs wait forever on a check nobody reports any more.
  Approval adds the label through `add-run-all-workflows-label.yml`, whose `labeled` event is what
  actually re-triggers `tests.yml`.

The macOS job **replaced** Linux `Dev`/clang instead of being added beside it, so the full matrix
is still four jobs. Nothing goes unchecked: clang at `-O3` with libstdc++ and mold stays in
`DevOpt`/clang, gcc keeps both build types, and coverage is untouched. What the macOS job adds is a
second standard library (libc++), the Apple arm64 ABI, `ld64` and a far newer clang.

The two platforms prepare a build so differently that neither preparation is in `tests.yml` at
all. It lives in `.github/actions/`, in four composite actions, one per platform per concern:

| | |
|---|---|
| `setup-linux-build-env/` | the `apt` wall and the ccache binary |
| `setup-macos-build-env/` | Homebrew toolchain env, the machine-local ccache, the python venv |
| `setup-linux-rust/` | the `apt` wall for the crates' native libraries, and the sccache wrapper |
| `setup-macos-rust/` | asserts the per-account rustup, and points sccache at the shared directory |

All four are picked by an `if:` on the caller, and none contains a platform check. Rust briefly
did it the other way -- one `setup-rust/` taking a `platform` input and branching inside, on the
grounds that Rust is the same dependency either way and only its delivery differs. In practice
every step in it carried `if: ${{ inputs.platform != 'macos' }}`, and the two halves shared one
line of actual behaviour, so the branching was the action. Splitting it is what the two
build-env actions were already doing. (`setup-rust/` had in turn replaced
`setup-duck-dependencies/`, which `rust.yml` had grown its own copy of.)

Both Rust actions put the toolchain on `PATH` themselves -- rustup installs per account into
`~/.cargo` on either platform. The entry is written `$HOME/.cargo/bin` rather than `~/.cargo/bin`,
because `GITHUB_PATH` lines are taken verbatim and nothing expands a tilde inside `PATH`. One job
does not use either action: `cargo fmt` parses the sources and compiles nothing, so it wants
neither the native libraries nor the sccache wrapper, and that one `PATH` line is its whole setup.

That is what keeps the steps after them readable as "build, then test" instead of a column of
`if: ${{ env.IS_MACOS != 'true' }}`. All four take everything they need through `with:`, because
secrets are not inherited by a composite action and the `env` context is not dependable inside
one -- and every `run` in a composite action needs an explicit `shell:`.

`toolbox.py workflows-lint` parses these alongside the workflow files. A broken composite action
does surface on the runner, unlike a broken workflow file, but there is no reason to learn from a
job what a YAML parse says locally.

Three things about the mac runner are worth knowing before editing any of it.

**Its accounts have no `sudo`**, so nothing there can install anything. The toolchain is whatever
is already on the machine -- Homebrew's `llvm@23` to compile, `llvm@19` to link against, ICU,
cmake, ninja, ccache, plus a per-account rustup in `~/.cargo`. `setup-macos-build-env` therefore
only ever *points at* things, and asserts the ones whose absence would otherwise surface much
later. A new build dependency has to be put on the machine by hand first.

**Its caches never travel through `actions/cache`.** On the Linux runners the ccache and sccache
directories are uploaded and downloaded per run, and old entries have to be pruned so they do not
snowball -- that is the `prune-caches` job, which runs `prune-caches.py` once after the whole
matrix rather than once per matrix entry, and talks to the REST API rather than to `gh`. On the mac there is only one machine, it is not containerised, and its disk
persists between jobs -- so the caches simply live there, under `/Users/Shared/duckling-ci`, and
every `actions/cache` step in `build` is Linux-only. Two consequences:

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
a `brew install rust` would leave the `duck` integration tests failing on a missing `env`. It is
installed per account, which is why `setup-macos-rust` asserts it rather than assuming it.

**The python venv and the downloaded binaries are not rebuilt every run.** The workspace is wiped
at the start of every job, so anything inside it is re-fetched each time. The venv's only input is
`requirements.txt`, so on macOS it lives in `$HOME` keyed by that file's hash, with `dev/.venv` a
symlink to it -- the symlink is what makes `toolbox.py`'s `setup_venv_impl` skip building a second
one in the workspace. Separately, `toolbox.py download-binaries` is now platform-aware: it used to
fetch a `linux-x86_64` ccache unconditionally, so a mac downloaded a binary it could not run
(harmlessly -- `find_program` searches `PATHS` after `PATH`, so Homebrew's ccache was picked and
the download simply sat unused).

#### Which Rust jobs run where

`rust.yml`'s `cargo test` runs on **both** platforms. `cargo clippy` and `cargo fmt` stay on Linux
alone: both read the same sources through the same pinned toolchain wherever they run, so a second
copy would report the same lints twice. What the mac adds is the Apple arm64 target, `ld64` and the
macOS halves of the `#[cfg]`s -- and only compiling and running the tests exercises those.

Its `runs-on` deliberately does **not** go through `determine-runners` the way the Linux jobs do.
That workflow falls back to a GitHub-hosted runner when the self-hosted one is busy, and a
GitHub-hosted mac would fail in `setup-macos-rust`, which asserts a per-account rustup with an `sccache`
next to it -- something this machine has only because it was put there by hand. So the macOS entry
names `{group: SelfHostedRunners, labels: macOS}` directly, exactly as `tests.yml`'s `build` does,
and queues for the machine rather than running somewhere unprepared.

The Linux entry keeps the job name it had when the job had no platform axis at all, and the macOS
entry is `cargo test / (duck & quackpack, dev/src/duck/, macOS)`. The same rule as for the build
matrix applies, for the same reason: the check name is the JOB name, so the Linux required check is
the string it always was and the mac is one new required check beside it.

Every `actions/cache` step in the job is Linux-only, for the reason given above -- on the mac
sccache lives in `/Users/Shared/duckling-ci/sccache-<account>` and simply stays there. It does not
need a longer `timeout-minutes` for that, though: measured on the machine, `cargo --locked test`
takes 32s with the sccache directory deleted (540 compilations, not one hit) and 19s with it warm,
against the same ten minutes Linux gets. Only an account's very first run, which also fetches the
toolchain and the crates, is anywhere near it.

Nothing had to be installed on the mac for this, which was checked by running the job's `cargo
--locked test` there by hand, with `PATH` cut down to what `setup-macos-rust` alone guarantees -- 225
tests, no Homebrew on `PATH`. The crates that would need a system library find one on their own:
`rusqlite` and `russcip` are bundled (`scip-sys` ships a prebuilt `macos-arm` SCIP), `libgit2-sys`
and `libssh2-sys` fall back to their vendored sources, `curl-sys` uses the system libcurl, and
`openssl-sys` resolves Homebrew's keg-only `openssl@3` through its own `/opt/homebrew/opt` lookup
without needing `PKG_CONFIG_PATH`. `rust-toolchain.toml` is honoured the same way as on Linux:
rustup on the mac had only `stable` installed and fetched 1.90.0 on its own.

### Background steps

Since 2026-06-25 a step may carry `background: true`, which starts it and moves straight on to the
next step; `wait` / `wait-all` block on named background steps, and `parallel` is shorthand for a
group of them with a `wait` at the end. Both `run:` and `uses:` steps can be backgrounded.

`tests.yml` uses it for the two `actions/cache/save` steps, so the tests do not sit waiting for an
upload. Three things decided how:

* **There is no explicit `wait`.** A `wait` step does not support `if:`, and both saves are
  skipped on macOS. The implicit `wait-all` that runs before post-job cleanup covers them, and a
  failed upload still fails the job there.
* **Position still matters.** A cache save must not overlap anything writing into the directory it
  is reading, which is why the build cache is saved after the last step that compiles through
  ccache rather than as early as possible.
* **Not available inside a composite action** -- `background` and `parallel` are workflow-level
  only, so `setup-linux-build-env` and `setup-macos-build-env` cannot use them.

A self-hosted runner has to be new enough to understand the keyword.

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
  > branch falls back to that entry, its size is everyone's download time. This used to be done by
  > downloading the entry and deleting the directory again before the build.

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
