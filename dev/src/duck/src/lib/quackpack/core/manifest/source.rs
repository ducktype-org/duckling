//! Dependencies' sources and interning.
use std::collections::HashSet;
use std::ops::Deref;
use std::path::{Path, PathBuf};
use std::sync::{Mutex, OnceLock};

use git2::FetchOptions;
use serde::{Deserialize, Serialize};
use url::Url;

use crate::quackpack::schemas::registry;
use crate::util::extract::Extract;
use crate::{QuackError, StrId, qp_bail};

static INTERNED_SOURCE_CACHE: OnceLock<Mutex<HashSet<&'static Source>>> = OnceLock::new();

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
/// An interned version of [`Source`].
pub struct InternedSource {
    inner: &'static Source,
}

impl InternedSource {
    /// Create a new [`InternedSource`].
    pub fn new(source: Source) -> Self {
        let mut cache = INTERNED_SOURCE_CACHE
            .get_or_init(Default::default)
            .lock()
            .extract();
        let reference = cache.get(&source).copied().unwrap_or_else(|| {
            let static_ref = Box::leak(Box::new(source));
            cache.insert(static_ref);
            static_ref
        });
        Self { inner: reference }
    }
}

impl Serialize for InternedSource {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        self.inner.serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for InternedSource {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        let source = Source::deserialize(deserializer)?;
        Ok(source.into())
    }
}

impl From<Source> for InternedSource {
    fn from(value: Source) -> Self {
        Self::new(value)
    }
}

impl Deref for InternedSource {
    type Target = Source;

    fn deref(&self) -> &'static Self::Target {
        self.inner
    }
}

impl AsRef<Source> for InternedSource {
    fn as_ref(&self) -> &'static Source {
        self.inner
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
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

#[derive(Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
/// Represents a source of a package which should be fetched from a registry.
pub struct Registry {
    url: Url,
}

impl std::fmt::Debug for Registry {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Registry")
            .field("url", &self.url.as_str())
            .finish()
    }
}

impl Registry {
    /// Create a new [`Registry`] source.
    pub fn new(url: Url) -> Self {
        Self { url }
    }

    /// Get the registry [`Url`].
    pub fn url(&self) -> &Url {
        &self.url
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
/// Represents a source of a local dependency, which lives on a disk.
pub struct Local {
    absolute: PathBuf,
    entry_in_manifest: StrId,
    was_original_entry_relative: bool,
}

impl Local {
    /// Create a new [`Local`] source.
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

#[derive(Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
/// Represents a source of a dependency cloned from git.
pub struct Git {
    url: Url,
    branch_or_tag: BranchOrTag,
    rev: Option<String>,
}

impl std::fmt::Debug for Git {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Git")
            .field("url", &self.url.as_str())
            .field("branch_or_tag", &self.branch_or_tag)
            .field("rev", &self.rev)
            .finish()
    }
}

impl Git {
    /// Create a new [`Git`] source.
    pub fn new(url: Url, branch_or_tag: BranchOrTag, rev: Option<String>) -> Self {
        Self {
            url,
            branch_or_tag,
            rev,
        }
    }

    /// Get the git repository URL.
    pub fn url(&self) -> &Url {
        &self.url
    }

    /// Get the git branch or tag.
    pub fn branch_or_tag(&self) -> &BranchOrTag {
        &self.branch_or_tag
    }

    /// Get the specific revision (commit hash), if any.
    pub fn rev(&self) -> Option<&str> {
        self.rev.as_deref()
    }

    /// Check whether we can perform a shallow clone of this dependency.
    ///
    /// Due to some git2-rs stuff we can't shallow clone a tag or a local repository.
    ///
    /// However, we always disallow shallow clones when commit is specified.
    pub fn can_shallow_clone(&self) -> bool {
        let is_local_repository_url = self.url.scheme() == "file";
        !is_local_repository_url && !self.branch_or_tag().is_tag() && self.rev.is_none()
    }

    /// Get [`FetchOptions`] for this source.
    ///
    /// This if factored out so we can easily make small changes to the
    /// [`FetchOptions`], such as setting depth to 0 to make a full fetch.
    pub fn git_fetch_options(&self) -> FetchOptions<'_> {
        let mut fetch_options = FetchOptions::new();
        if self.can_shallow_clone() {
            fetch_options.depth(1);
        }
        fetch_options
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
/// A type-safe approach for specifying a git tag or a branch.
pub enum BranchOrTag {
    /// The default branch.
    Default,
    /// A specific tag.
    Tag(String),
    /// A specific branch.
    Branch(String),
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

impl TryFrom<registry::DependencySource> for InternedSource {
    type Error = QuackError;

    fn try_from(value: registry::DependencySource) -> Result<Self, Self::Error> {
        Ok(Self::new(value.try_into()?))
    }
}

impl TryFrom<InternedSource> for registry::DependencySource {
    type Error = QuackError;

    fn try_from(value: InternedSource) -> Result<Self, Self::Error> {
        value.try_into()
    }
}

impl TryFrom<registry::DependencySource> for Source {
    type Error = QuackError;

    fn try_from(value: registry::DependencySource) -> Result<Self, Self::Error> {
        let tmp = match value.inner {
            registry::SourceInner::Registry { registry_url } => Registry {
                url: registry_url.as_str().try_into()?,
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
                    (None, Some(branch)) => BranchOrTag::Branch(branch),
                    (Some(tag), None) => BranchOrTag::Tag(tag),
                    (Some(_), Some(_)) => {
                        qp_bail!("git dependency in the registry specifies both `tag` and `branch`")
                    }
                };
                Git {
                    url: git_url.as_str().try_into()?,
                    branch_or_tag,
                    rev: commit,
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
                    BranchOrTag::Tag(tag) => (Some(tag), None),
                    BranchOrTag::Branch(branch) => (None, Some(branch)),
                };
                registry::SourceInner::Git {
                    git_url: url.into(),
                    commit: rev,
                    tag,
                    branch,
                }
            }
        };
        Ok(Self { inner })
    }
}
