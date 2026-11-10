// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module responsible for interacting with git repositories.
//!
//! > [!IMPORTANT]
//! > Because we work with [`GitClient`] behind a `Box<dyn>`, we can't have associated types.
//! > Therefore, we are using type-erased `Box<dyn Any>`, with downcasting. It's *very* important
//! > not to mix [`GitRepository`] nor [`GitCommit`] between different types (or even instances) of
//! > implementers of [`GitClient`].

use std::any::{Any, type_name};
use std::fmt;
use std::path::Path;

use url::Url;

use crate::quackpack::core::GitReference;
use crate::quackpack::util::is_local_file::IsLocalFile;
use crate::util::dyn_type_name::DynTypeName;
use crate::{QuackResult, StrId, qp_bail_internal};

pub mod cli;
pub mod fast_path;
pub mod git2;
mod known_hosts;
#[cfg(test)]
mod tests;

#[derive(Debug)]
/// A type-erased client-specific repository.
pub struct GitRepository {
    inner: Box<dyn Any>,
}

#[derive(Debug)]
/// A type-erased client-specific commit.
pub struct GitCommit {
    inner: Box<dyn Any>,
}

/// Generic git client.
pub trait GitClient: fmt::Debug {
    /// Clone a repository to the given path.
    fn clone(
        &self,
        url: &Url,
        reference: GitReference,
        destination: &Path,
    ) -> QuackResult<GitRepository>;

    /// Checkout a given repository, to the reference.
    fn checkout(&self, repository: &GitRepository, reference: GitReference) -> QuackResult<()>;

    /// Get current HEAD commit.
    fn get_head_commit(&self, repository: &GitRepository) -> QuackResult<GitCommit>;

    /// Get current HEAD commit.
    fn commit_to_str(&self, commit: &GitCommit) -> QuackResult<StrId>;

    /// Returns `true` if the client uses git CLI.
    fn uses_cli(&self) -> bool;
}

/// Check, if we can shallow clone a `reference` from `url`.
/// Right now conditions are as follow:
/// 1. `url` must not be a local repository (i.e. schema != "file"),
/// 2. reference must NOT point to specific commit (it's either a default branch, or a specific branch).
fn can_shallow_clone(url: &Url, reference: GitReference) -> bool {
    let is_local_repository_url = url.is_local_file();
    !is_local_repository_url && (reference.is_default() || reference.is_branch())
}

/// A helper trait for implementers.
trait AnyExt {
    fn downcast_ext<T: Any>(&self) -> QuackResult<&T>;
}

impl AnyExt for dyn Any {
    fn downcast_ext<T: Any>(&self) -> QuackResult<&T> {
        if let Some(downcasted) = self.downcast_ref::<T>() {
            return Ok(downcasted);
        };

        let expected = type_name::<T>();
        let name = self.type_name();
        let id = self.type_id();
        qp_bail_internal!("invalid use of `AnyExt`: expected `{expected}`, got `{name}` ({id:?})")
    }
}
