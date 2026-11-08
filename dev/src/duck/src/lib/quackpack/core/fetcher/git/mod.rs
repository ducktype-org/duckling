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
use crate::{QuackResult, StrId};

pub mod cli;
pub mod fast_path;
pub mod git2;
mod known_hosts;
#[cfg(test)]
mod tests;

#[derive(Debug)]
pub struct ClonedRepositoryData {
    /// Hash of the checkout'd repository.
    pub commit: StrId,
}

/// Generic git client.
pub trait GitClient: fmt::Debug {
    /// Clone a repository to the given path.
    fn clone(
        &self,
        url: &Url,
        reference: GitReference,
        destination: &Path,
    ) -> QuackResult<ClonedRepositoryData>;

    /// Returns `true` if the client uses git CLI.
    fn uses_cli(&self) -> bool;
}
