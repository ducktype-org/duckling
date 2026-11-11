// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! CLI based git client.

use std::io;
use std::path::{Path, PathBuf};
use std::process::{Command, Output};

use tracing::{debug, trace, warn};
use url::Url;

use super::{GitClient, GitRepository, GitSubmoduleOptions, can_shallow_clone};
use crate::quackpack::core::GitReference;
use crate::util::command_ext::CommandExt;
use crate::{QuackError, QuackResult, QuackResultContext, StrId, qp_bail};

#[derive(Debug)]
/// A git repository rooted at the given path.
struct GitCliRepository(PathBuf);

/// git environmental variables to unset.
/// They can affect git execution.
const GIT_ENVS_TO_CLEAR: &[&str] = &[
    "GIT_DIR",
    "GIT_WORK_TREE",
    "GIT_INDEX_FILE",
    "GIT_OBJECT_DIRECTORY",
    "GIT_ALTERNATE_OBJECT_DIRECTORIES",
];

#[derive(Debug, Clone, Copy)]
// Private field to disallow manual creation.
pub struct GitCliClient(());

impl GitCliClient {
    /// Create a new [`GitCliClient`].
    ///
    /// This function will check that git binary exists.
    pub fn new() -> QuackResult<Self> {
        check_git_binary_exists()?;
        Ok(Self(()))
    }
}

impl GitClient for GitCliClient {
    fn clone(
        &self,
        url: &Url,
        reference: GitReference,
        destination: &Path,
    ) -> QuackResult<Box<dyn GitRepository>> {
        clone_git_repository(url, reference, destination)?;
        let repository = GitCliRepository(destination.to_path_buf());
        Ok(Box::new(repository))
    }

    fn uses_cli(&self) -> bool {
        true
    }
}

impl GitRepository for GitCliRepository {
    fn checkout(&self, reference: GitReference) -> QuackResult<()> {
        checkout(reference, &self.0)
    }

    fn head_commit(&self) -> QuackResult<StrId> {
        get_current_commit(&self.0)
    }

    fn init_submodules(&self, options: GitSubmoduleOptions) -> QuackResult<()> {
        initialize_submodules(options, &self.0)
    }
}

/// Check that git binary exists.
fn check_git_binary_exists() -> QuackResult<()> {
    let mut command = git_cmd();
    command.arg("--version");
    let output = get_git_output(&mut command).map_err(add_path_hint_to_missing_git)?;
    debug!(target: "duck::git", version = %String::from_utf8_lossy(&output.stdout), "git binary version");
    Ok(())
}

/// Clone a git repository via [`Command`].
fn clone_git_repository(url: &Url, reference: GitReference, destination: &Path) -> QuackResult<()> {
    let mut command = git_cmd();
    command.arg("clone");
    if let GitReference::Branch(branch_or_tag) | GitReference::Tag(branch_or_tag) = reference {
        command.arg(format!("--branch={branch_or_tag}"));
    }
    if can_shallow_clone(url, reference) {
        command.arg("--depth=1");
    }
    command.arg(url.as_str()).arg(destination);
    execute_git(&mut command)
}

/// Checkout repo to the given reference.
fn checkout(reference: GitReference, path: &Path) -> QuackResult<()> {
    if let GitReference::Rev(commit) = reference {
        validate_commit(commit, path)?;
    }
    if let GitReference::Tag(tag) = reference {
        validate_tag(tag, path)?;
    }
    match reference {
        // We only checkout to tag or commit, since we can clone to branch.
        GitReference::Tag(tag_or_commit) | GitReference::Rev(tag_or_commit) => {
            let mut command = git_cmd();
            set_cwd_on_git_cmd(&mut command, path)
                .arg("checkout")
                .arg(tag_or_commit);
            execute_git(&mut command)
        }
        GitReference::Branch(_) | GitReference::Default => Ok(()),
    }
}

/// Initialize git submodules, following `options`.
fn initialize_submodules(options: GitSubmoduleOptions, path: &Path) -> QuackResult<()> {
    let mut command = git_cmd();
    set_cwd_on_git_cmd(&mut command, path);
    command.arg("submodule");
    match options {
        GitSubmoduleOptions::Init => {
            command.arg("init");
        }
        GitSubmoduleOptions::Update { init, recursive } => {
            command.arg("update");
            if init {
                command.arg("--init");
            }
            if recursive {
                command.arg("--recursive");
            }
        }
    }
    execute_git(&mut command)
}

fn validate_commit(commit: StrId, path: &Path) -> QuackResult<()> {
    let mut command = git_cmd();
    set_cwd_on_git_cmd(&mut command, path)
        .args(["rev-parse", "--verify", "--quiet", "--end-of-options"])
        .arg(format!("{}^{{commit}}", commit));
    let _ = get_git_output(&mut command)
        .with_context(|| format!("`{commit}` is not a valid commit"))?;
    Ok(())
}

fn validate_tag(tag: StrId, path: &Path) -> QuackResult<()> {
    let mut command = git_cmd();
    set_cwd_on_git_cmd(&mut command, path)
        .arg("check-ref-format")
        .arg(format!("refs/tags/{tag}"));
    let _ = get_git_output(&mut command).with_context(|| format!("`{tag}` is not a valid tag"))?;
    Ok(())
}

/// Get current HEAD commit.
fn get_current_commit(path: &Path) -> QuackResult<StrId> {
    let mut command = git_cmd();
    set_cwd_on_git_cmd(&mut command, path).args(["rev-parse", "HEAD"]);
    let output = get_git_output(&mut command)?;
    let commit = String::from_utf8(output.stdout)
        .with_context(|| format!("`{}` returned invalid UTF8", command.display()))?;
    Ok(commit.trim().into())
}

/// Set new CWD for git.
fn set_cwd_on_git_cmd<'a>(command: &'a mut Command, cwd: &Path) -> &'a mut Command {
    command.current_dir(cwd);
    if cwd.is_relative() {
        warn!(
            ?cwd,
            "using relative path for subcommand cwd may have weird side-effects"
        );
    }
    command
}

fn git_cmd() -> Command {
    let mut command = Command::new("git");
    for env in GIT_ENVS_TO_CLEAR {
        command.env_remove(env);
    }
    command
}

/// Execute a prepared git command, inheriting stdout, stderr, and stdin.
fn execute_git(command: &mut Command) -> QuackResult<()> {
    trace!(target: "duck::git", ?command, "executing git command");
    let status = command
        .status()
        .with_context(|| format!("failed to execute `{}`", command.display()))?;
    if !status.success() {
        qp_bail!("failed to execute `{}`", command.display())
    }
    Ok(())
}

/// Execute a prepared git command, and return the output.
fn get_git_output(command: &mut Command) -> QuackResult<Output> {
    trace!(target: "duck::git", ?command, "executing git command");
    let output = command
        .output()
        .with_context(|| format!("failed to execute `{}`", command.display()))?;
    if !output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        let stderr = String::from_utf8_lossy(&output.stderr);
        qp_bail!(
            "failed to execute `{}`\nstdout: {}\nstderr: {}",
            command.display(),
            stdout.trim(),
            stderr.trim()
        )
    }
    Ok(output)
}

/// Add a hint to add `git` to the `$PATH`, if it was not found.
fn add_path_hint_to_missing_git(mut error: QuackError) -> QuackError {
    if let Some(io_err) = error.downcast_ref_in_chain::<io::Error>()
        && io_err.kind() == io::ErrorKind::NotFound
    {
        error.add_hint("try adding `git` to the $PATH");
        error
    } else {
        error
    }
}
