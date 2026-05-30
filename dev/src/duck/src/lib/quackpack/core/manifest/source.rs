//! Dependencies' sources and interning.
use std::collections::HashSet;
use std::ops::Deref;
use std::path::{Path, PathBuf};
use std::sync::{Mutex, OnceLock};

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
    reference: GitReference,
}

impl std::fmt::Debug for Git {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Git")
            .field("url", &self.url.as_str())
            .field("reference", &self.reference)
            .finish()
    }
}

impl Git {
    /// Create a new [`Git`] source.
    pub fn new(url: Url, reference: GitReference) -> Self {
        Self { url, reference }
    }

    /// Get the git repository URL.
    pub fn url(&self) -> &Url {
        &self.url
    }

    /// Get the git branch or tag.
    pub fn reference(&self) -> &GitReference {
        &self.reference
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
/// A type-safe approach for specifying a git tag or a branch.
// We intentionally keep inner values as strings: we don't clone them a lot,
// and turning them into StrId would only “leak” memory.
pub enum GitReference {
    /// The default branch.
    Default,
    /// A specific tag.
    Tag(String),
    /// A specific branch.
    Branch(String),
    /// Other.
    Rev(String),
}

impl GitReference {
    /// Helper around `matches!(self, GitReference::Default)`.
    pub fn is_default(&self) -> bool {
        matches!(self, GitReference::Default)
    }

    /// Helper around `matches!(self, GitReference::Tag(..))`.
    pub fn is_tag(&self) -> bool {
        matches!(self, GitReference::Tag(..))
    }

    /// Helper around `matches!(self, GitReference::Branch(..))`.
    pub fn is_branch(&self) -> bool {
        matches!(self, GitReference::Branch(..))
    }

    /// Helper around `matches!(self, GitReference::Rev(..))`.
    pub fn is_rev(&self) -> bool {
        matches!(self, GitReference::Rev(..))
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
                let reference = match (tag, branch, commit) {
                    (None, None, None) => GitReference::Default,
                    (None, Some(branch), None) => GitReference::Branch(branch),
                    (Some(tag), None, None) => GitReference::Tag(tag),
                    (None, None, Some(commit)) => GitReference::Rev(commit),
                    _ => {
                        qp_bail!("only one of `branch`, `tag`, or `commit` can be specified")
                    }
                };
                Git {
                    url: git_url.as_str().try_into()?,
                    reference,
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
                let Git { url, reference } = git;
                let (tag, branch, commit) = match reference {
                    GitReference::Default => (None, None, None),
                    GitReference::Tag(tag) => (Some(tag), None, None),
                    GitReference::Branch(branch) => (None, Some(branch), None),
                    GitReference::Rev(commit) => (None, None, Some(commit)),
                };
                registry::SourceInner::Git {
                    git_url: url.into(),
                    commit,
                    tag,
                    branch,
                }
            }
        };
        Ok(Self { inner })
    }
}
