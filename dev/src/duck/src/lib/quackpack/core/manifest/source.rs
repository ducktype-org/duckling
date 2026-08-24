//! Dependencies' sources and interning.
use std::path::Path;

use serde::{Deserialize, Serialize};

use crate::quackpack::core::full_identity::{FullKind, FullOrigin};
use crate::quackpack::schemas::registry;
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::is_local_file::IsLocalFile;
use crate::quackpack::util::to_url::ToUrl;
use crate::{QuackError, QuackResult, StrId, qp_bail};

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash, Serialize, Deserialize)]
pub enum SourceKind {
    Registry,
    Local,
    Git(GitReference),
}

impl SourceKind {
    pub fn is_registry(self) -> bool {
        matches!(self, SourceKind::Registry)
    }

    pub fn is_local(self) -> bool {
        matches!(self, SourceKind::Local)
    }

    pub fn is_git(self) -> bool {
        matches!(self, SourceKind::Git(..))
    }

    pub fn maybe_reference(self) -> Option<GitReference> {
        if let SourceKind::Git(reference) = self {
            Some(reference)
        } else {
            None
        }
    }
}

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash, Serialize, Deserialize)]
pub struct Source {
    kind: SourceKind,
    url: InternedUrl,
}

impl Source {
    fn new(url: InternedUrl, kind: SourceKind) -> Self {
        // kind = local => url.is_local_file
        debug_assert!(
            url.is_local_file() || !kind.is_local(),
            "kind=`local` should have `file://`; has kind=`{kind:?}` and url=`{url}`"
        );
        Self { url, kind }
    }

    /// Create a new [`Source`] for a registry.
    pub fn for_registry(url: impl Into<InternedUrl>) -> Self {
        Self::new(url.into(), SourceKind::Registry)
    }

    /// Create a new [`Source`] for a git with a reference.
    pub fn for_git(url: impl Into<InternedUrl>, reference: GitReference) -> Self {
        Self::new(url.into(), SourceKind::Git(reference))
    }

    /// Create a new [`Source`] for a local package.
    pub fn for_local(root: &Path) -> QuackResult<Self> {
        let url = root.to_url()?;
        Ok(Self::new(url.into(), SourceKind::Local))
    }

    /// Create a new [`Source`] for a local package, but the path to the package is a url.
    pub fn for_local_with_url(root: impl Into<InternedUrl>) -> Self {
        Self::new(root.into(), SourceKind::Local)
    }

    /// Get an [`InternedUrl`] of this [`Source`].
    pub fn url(self) -> InternedUrl {
        self.url
    }

    /// Get a [`SourceKind`] of this [`Source`].
    pub fn kind(self) -> SourceKind {
        self.kind
    }

    /// Helper for `source.kind().is_registry()`.
    pub fn is_registry(self) -> bool {
        self.kind.is_registry()
    }

    /// Helper for `source.kind().is_git()`.
    pub fn is_git(self) -> bool {
        self.kind.is_git()
    }

    /// Helper for `source.kind().is_local()`.
    pub fn is_local(self) -> bool {
        self.kind.is_local()
    }

    /// Helper for `source.kind().maybe_reference()`.
    pub fn maybe_reference(self) -> Option<GitReference> {
        self.kind.maybe_reference()
    }

    /// Creates a [`Source`] which could correspond to the given [`FullOrigin`].
    /// Used for generating mapping Source -> FullOrigin for packages from the previous freeze.
    /// Note that this mapping is many-to-one.
    pub fn canonical_source_for_origin(origin: FullOrigin) -> Self {
        match origin.kind() {
            FullKind::Registry => Self::for_registry(origin.url()),
            FullKind::Git { commit } => Self::for_git(origin.url(), GitReference::Rev(commit)),
            FullKind::Local => Self::for_local_with_url(origin.url()),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
/// A type-safe approach for specifying a git reference.
pub enum GitReference {
    /// The default branch.
    Default,
    /// A specific tag.
    Tag(StrId),
    /// A specific branch.
    Branch(StrId),
    /// Other.
    Rev(StrId),
}

impl GitReference {
    /// Helper around `matches!(self, GitReference::Default)`.
    pub fn is_default(self) -> bool {
        matches!(self, GitReference::Default)
    }

    /// Helper around `matches!(self, GitReference::Tag(..))`.
    pub fn is_tag(self) -> bool {
        matches!(self, GitReference::Tag(..))
    }

    /// Helper around `matches!(self, GitReference::Branch(..))`.
    pub fn is_branch(self) -> bool {
        matches!(self, GitReference::Branch(..))
    }

    /// Helper around `matches!(self, GitReference::Rev(..))`.
    pub fn is_rev(self) -> bool {
        matches!(self, GitReference::Rev(..))
    }
}

impl TryFrom<registry::DependencySource> for Source {
    type Error = QuackError;

    fn try_from(value: registry::DependencySource) -> Result<Self, Self::Error> {
        let tmp = match value.inner {
            registry::SourceInner::Registry { registry_url } => {
                let url = registry_url.as_str().to_url()?;
                Self::for_registry(url)
            }
            registry::SourceInner::Git {
                git_url,
                commit,
                tag,
                branch,
            } => {
                let url = git_url.as_str().to_url()?;
                let reference = match (tag, branch, commit) {
                    (None, None, None) => GitReference::Default,
                    (None, Some(branch), None) => GitReference::Branch(branch.into()),
                    (Some(tag), None, None) => GitReference::Tag(tag.into()),
                    (None, None, Some(commit)) => GitReference::Rev(commit.into()),
                    _ => {
                        qp_bail!("only one of `branch`, `tag`, or `commit` can be specified")
                    }
                };
                Self::for_git(url, reference)
            }
        };
        Ok(tmp)
    }
}

impl TryFrom<Source> for registry::DependencySource {
    type Error = QuackError;
    fn try_from(value: Source) -> Result<Self, Self::Error> {
        let kind = value.kind;
        let url = value.url;
        let inner = match kind {
            SourceKind::Registry => registry::SourceInner::Registry {
                registry_url: url.to_string(),
            },
            SourceKind::Local => qp_bail!("publishing of local dependencies is not yet supported"),
            SourceKind::Git(reference) => {
                let (tag, branch, commit) = match reference {
                    GitReference::Default => (None, None, None),
                    GitReference::Tag(tag) => (Some(tag.into()), None, None),
                    GitReference::Branch(branch) => (None, Some(branch.into()), None),
                    GitReference::Rev(commit) => (None, None, Some(commit.into())),
                };
                registry::SourceInner::Git {
                    git_url: url.to_string(),
                    commit,
                    tag,
                    branch,
                }
            }
        };
        Ok(Self { inner })
    }
}
