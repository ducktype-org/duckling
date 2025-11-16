use std::path::{Path, PathBuf};

use crate::StrId;

#[derive(Debug, Clone, Copy)]
/// Represents a source of a package which should be fetched from a registry.
pub struct Registry {
    url: StrId,
}

impl Registry {
    /// Create a new registry source.
    pub fn new(url: StrId) -> Self {
        Self { url }
    }

    /// Get the registry URL.
    pub fn url(&self) -> StrId {
        self.url
    }
}

#[derive(Debug, Clone)]
/// Represents a source of a local dependency, which lives on a disk.
pub struct Local {
    absolute: PathBuf,
    _entry_in_manifest: StrId,
}

impl Local {
    /// Create a new local source.
    pub fn new(absolute: PathBuf, entry_in_manifest: StrId) -> Self {
        Self {
            absolute,
            _entry_in_manifest: entry_in_manifest,
        }
    }

    /// Get the absolute path to the local package.
    pub fn absolute(&self) -> &Path {
        &self.absolute
    }
}

#[derive(Debug, Clone, Copy)]
/// A type-safe approach for specifying a git tag or a branch.
pub enum GitRevision {
    /// The main branch.
    Main,
    /// A specific tag.
    Tag(StrId),
    /// A specific branch.
    Branch(StrId),
}

impl GitRevision {
    /// Helper around `matches!(self, GitRevision::Main)`.
    pub fn is_main(&self) -> bool {
        matches!(self, GitRevision::Main)
    }

    /// Helper around `matches!(self, GitRevision::Tag(..))`.
    pub fn is_tag(&self) -> bool {
        matches!(self, GitRevision::Tag(..))
    }

    /// Helper around `matches!(self, GitRevision::Branch(..))`.
    pub fn is_branch(&self) -> bool {
        matches!(self, GitRevision::Branch(..))
    }
}

#[derive(Debug, Clone, Copy)]
/// Represents a source a dependency cloned from git.
pub struct Git {
    url: StrId,
    rev: GitRevision,
    commit: Option<StrId>,
}

impl Git {
    /// Create a new git source.
    pub fn new(url: StrId, rev: GitRevision, commit: Option<StrId>) -> Self {
        Self { url, rev, commit }
    }

    /// Get the git repository URL.
    pub fn url(&self) -> StrId {
        self.url
    }

    /// Get the git revision.
    pub fn rev(&self) -> GitRevision {
        self.rev
    }

    /// Get the specific commit hash, if any.
    pub fn commit(&self) -> Option<StrId> {
        self.commit
    }
}

#[derive(Debug, Clone)]
/// General dependency source.
pub enum Source {
    /// A package from a registry.
    Registry(Registry),
    /// A local package on disk.
    Local(Local),
    /// A package from a git repository.
    Git(Git),
}

impl Source {
    /// Helper around `matches!(self, Source::Git(..))`.
    pub fn is_git(&self) -> bool {
        matches!(self, Source::Git(..))
    }

    /// Helper around `matches!(self, Source::Local(..))`.
    pub fn is_local(&self) -> bool {
        matches!(self, Source::Local(..))
    }

    /// Helper around `matches!(self, Source::Registry(..))`.
    pub fn is_registry(&self) -> bool {
        matches!(self, Source::Registry(..))
    }
}
