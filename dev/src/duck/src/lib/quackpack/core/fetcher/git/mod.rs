// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module responsible for interacting with git repositories.

use std::fmt;
use std::path::Path;

use url::Url;

use crate::quackpack::core::GitReference;
use crate::quackpack::util::is_local_file::IsLocalFile;
use crate::{QuackResult, StrId};

pub mod cli;
pub mod fast_path;
pub mod git2;
mod known_hosts;
#[cfg(test)]
mod tests;

/// Generic git client.
pub trait GitClient: fmt::Debug {
    /// Clone a repository to the given path.
    fn clone(
        &self,
        url: &Url,
        reference: GitReference,
        destination: &Path,
    ) -> QuackResult<Box<dyn GitRepository>>;

    /// Returns `true` if the client uses git CLI.
    fn uses_cli(&self) -> bool;
}

/// Generic git repository.
pub trait GitRepository {
    /// Checkout a given repository, to the reference.
    ///
    /// Currently this only checkouts to tags or commits, since all implementers can clone to branch.
    fn checkout(&self, reference: GitReference) -> QuackResult<()>;

    /// Get current HEAD commit.
    fn head_commit(&self) -> QuackResult<StrId>;
}

/// Check, if we can shallow clone a `reference` from `url`.
/// Right now conditions are as follow:
/// 1. `url` must not be a local repository (i.e. schema != "file"),
/// 2. reference must NOT point to specific commit (it's either a default branch, or a specific branch).
fn can_shallow_clone(url: &Url, reference: GitReference) -> bool {
    let is_local_repository_url = url.is_local_file();
    !is_local_repository_url && (reference.is_default() || reference.is_branch())
}
