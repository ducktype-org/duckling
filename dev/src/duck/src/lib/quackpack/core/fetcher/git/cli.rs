// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! CLI based git client.

use std::io;
use std::path::Path;
use std::process::{Command, Output};

use tracing::{debug, trace};
use url::Url;

use super::{ClonedRepositoryData, GitClient};
use crate::quackpack::core::GitReference;
use crate::util::command_ext::CommandExt;
use crate::{QuackError, QuackResult, QuackResultContext, StrId, qp_bail};

#[derive(Debug, Clone, Copy)]
// Private field to disallow manual creation.
pub struct GitCliClient {
    // Private, unused field to disallow manual creation.
    _unused: (),
}

impl GitCliClient {
    /// Create a new [`GitCliClient`].
    ///
    /// This function will check that git binary exists.
    pub fn new() -> QuackResult<Self> {
        check_git_binary_exists()?;
        Ok(Self { _unused: () })
    }

    /// Clone a repository pointed by `url` into `destination`.
    #[tracing::instrument(target = "duck::git", level = "debug", skip(self, url), fields(%url, ?reference, ?destination))]
    pub fn clone(
        &self,
        url: &Url,
        reference: GitReference,
        destination: &Path,
    ) -> QuackResult<ClonedRepositoryData> {
        clone_git_repository(url, reference, destination)?;
        maybe_checkout_to_commit(reference, destination)?;
        let commit = get_current_commit(destination)?;
        Ok(ClonedRepositoryData { commit })
    }
}

impl GitClient for GitCliClient {
    fn clone(
        &self,
        url: &Url,
        reference: GitReference,
        destination: &Path,
    ) -> QuackResult<ClonedRepositoryData> {
        self.clone(url, reference, destination)
    }

    fn uses_cli(&self) -> bool {
        true
    }
}

// This function doesn't use helper, as we add a hint.
fn check_git_binary_exists() -> QuackResult<()> {
    let mut command = Command::new("git");
    command.arg("--version");
    let output = command
        .output()
        .context("failed to execute `git --version`")
        .map_err(add_path_hint_to_missing_git)?;
    if !output.status.success() {
        qp_bail!("failed to execute `git --version`")
    }
    debug!(target: "duck::git", version = %String::from_utf8_lossy(&output.stdout), "git binary version");
    Ok(())
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
        qp_bail!("failed to execute `{}`", command.display())
    }
    Ok(output)
}

/// Clone a git repository via [`Command`].
fn clone_git_repository(url: &Url, reference: GitReference, destination: &Path) -> QuackResult<()> {
    let mut command = Command::new("git");
    command.arg("clone");
    if let GitReference::Branch(branch_or_tag) | GitReference::Tag(branch_or_tag) = reference {
        command
            .arg("--depth=1")
            .arg(format!("--branch={branch_or_tag}"));
    }
    command.arg(url.as_str()).arg(destination);
    execute_git(&mut command)
}

/// Checkout repo to the given commit, if we requested that.
fn maybe_checkout_to_commit(reference: GitReference, path: &Path) -> QuackResult<()> {
    let GitReference::Rev(commit) = reference else {
        return Ok(());
    };
    let mut command = Command::new("git");
    command.arg("-C").arg(path).arg("checkout").arg(commit);
    execute_git(&mut command)
}

/// Get current HEAD commit.
fn get_current_commit(path: &Path) -> QuackResult<StrId> {
    let mut command = Command::new("git");
    command.arg("-C").arg(path).args(["rev-parse", "HEAD"]);
    let output = get_git_output(&mut command)?;
    let commit = String::from_utf8(output.stdout)
        .with_context(|| format!("`{}` returned invalid UTF8", command.display()))?;
    Ok(commit.trim().into())
}

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
