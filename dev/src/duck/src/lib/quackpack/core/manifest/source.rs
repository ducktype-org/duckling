use std::path::{Path, PathBuf};

use crate::quackpack::schemas::registry;
use crate::{QuackError, StrId, qp_bail};

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

impl From<Registry> for Source {
    fn from(val: Registry) -> Self {
        Source::Registry(val)
    }
}

impl From<Local> for Source {
    fn from(val: Local) -> Self {
        Source::Local(val)
    }
}

impl From<Git> for Source {
    fn from(val: Git) -> Self {
        Source::Git(val)
    }
}

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
    entry_in_manifest: StrId,
    was_original_entry_relative: bool,
}

impl Local {
    /// Create a new local source.
    pub fn new(
        absolute: PathBuf,
        entry_in_manifest: StrId,
        was_original_entry_relative: bool,
    ) -> Self {
        Self {
            absolute,
            entry_in_manifest,
            was_original_entry_relative,
        }
    }

    /// Get the absolute path to the local package.
    pub fn absolute(&self) -> &Path {
        &self.absolute
    }

    /// Get the entry which was directly specified in the manifest.
    pub fn entry_in_manifest(&self) -> StrId {
        self.entry_in_manifest
    }

    /// Whether [`entry_in_manifest`](Self::entry_in_manifest) was found to be a relative path.
    pub fn was_original_entry_relative(&self) -> bool {
        self.was_original_entry_relative
    }
}

#[derive(Debug, Clone, Copy)]
/// Represents a source a dependency cloned from git.
pub struct Git {
    url: StrId,
    branch_or_tag: BranchOrTag,
    rev: Option<StrId>,
}

impl Git {
    /// Create a new git source.
    pub fn new(url: StrId, branch_or_tag: BranchOrTag, rev: Option<StrId>) -> Self {
        Self {
            url,
            branch_or_tag,
            rev,
        }
    }

    /// Get the git repository URL.
    pub fn url(&self) -> StrId {
        self.url
    }

    /// Get the git branch or tag.
    pub fn branch_or_tag(&self) -> BranchOrTag {
        self.branch_or_tag
    }

    /// Get the specific revision (commit hash), if any.
    pub fn rev(&self) -> Option<StrId> {
        self.rev
    }

    /// Check whether we can perform a shallow clone of this dependency.
    ///
    /// Due to some git2-rs stuff we can't shallow clone a tag or a local repository.
    ///
    /// However, we always disallow shallow clones when commit is specified.
    pub fn can_shallow_clone(&self) -> bool {
        let looks_like_remote_url = self.url.starts_with("https://")
            || self.url.starts_with("git@")
            || self.url.starts_with("ssh://")
            || self.url.starts_with("http://")
            || self.url.starts_with("ftp://");
        looks_like_remote_url && !self.branch_or_tag().is_tag() && self.rev.is_none()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
/// A type-safe approach for specifying a git tag or a branch.
pub enum BranchOrTag {
    /// The default branch.
    Default,
    /// A specific tag.
    Tag(StrId),
    /// A specific branch.
    Branch(StrId),
}

impl BranchOrTag {
    /// Helper around `matches!(self, BranchOrTag::Default)`.
    pub fn is_default(&self) -> bool {
        matches!(self, BranchOrTag::Default)
    }

    /// Helper around `matches!(self, BranchOrTag::Tag(..))`.
    pub fn is_tag(&self) -> bool {
        matches!(self, BranchOrTag::Tag(..))
    }

    /// Helper around `matches!(self, BranchOrTag::Branch(..))`.
    pub fn is_branch(&self) -> bool {
        matches!(self, BranchOrTag::Branch(..))
    }
}

impl TryFrom<registry::DependencySource> for Source {
    type Error = QuackError;

    fn try_from(value: registry::DependencySource) -> Result<Self, Self::Error> {
        let tmp = match value.inner {
            registry::SourceInner::Registry { registry_url } => Registry {
                url: registry_url.into(),
            }
            .into(),
            registry::SourceInner::Local {
                absolute_dir_root,
                dir_entry_in_manifest,
            } => Local {
                absolute: absolute_dir_root.into(),
                entry_in_manifest: dir_entry_in_manifest.into(),
                was_original_entry_relative: false,
            }
            .into(),
            registry::SourceInner::Git {
                git_url,
                commit,
                tag,
                branch,
            } => {
                let branch_or_tag = match (tag, branch) {
                    (None, None) => BranchOrTag::Default,
                    (None, Some(branch)) => BranchOrTag::Branch(branch.into()),
                    (Some(tag), None) => BranchOrTag::Tag(tag.into()),
                    (Some(_), Some(_)) => {
                        qp_bail!("git dependency in the registry specifies both `tag` and `branch`")
                    }
                };
                Git {
                    url: git_url.into(),
                    branch_or_tag,
                    rev: commit.map(Into::into),
                }
                .into()
            }
        };
        Ok(tmp)
    }
}

impl TryFrom<Source> for registry::DependencySource {
    type Error = QuackError;
    fn try_from(value: Source) -> Result<Self, Self::Error> {
        let inner = match value {
            Source::Registry(registry) => registry::SourceInner::Registry {
                registry_url: registry.url.into(),
            },
            Source::Local(local) => {
                let Local {
                    absolute,
                    entry_in_manifest,
                    ..
                } = local;
                let absolute_dir_root = match absolute.into_os_string().into_string() {
                    Ok(absolute) => absolute,
                    Err(original) => qp_bail!(
                        "absolute path `{}` is not a utf-8 string",
                        original.display()
                    ),
                };
                registry::SourceInner::Local {
                    absolute_dir_root,
                    dir_entry_in_manifest: entry_in_manifest.into(),
                }
            }
            Source::Git(git) => {
                let Git {
                    url,
                    branch_or_tag,
                    rev,
                } = git;
                let (tag, branch) = match branch_or_tag {
                    BranchOrTag::Default => (None, None),
                    BranchOrTag::Tag(tag) => (Some(tag.into()), None),
                    BranchOrTag::Branch(branch) => (None, Some(branch.into())),
                };
                registry::SourceInner::Git {
                    git_url: url.into(),
                    commit: rev.map(Into::into),
                    tag,
                    branch,
                }
            }
        };
        Ok(Self { inner })
    }
}
