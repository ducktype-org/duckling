use std::collections::HashSet;
use std::ops::Deref;
use std::path::PathBuf;
use std::sync::{Mutex, OnceLock};

use serde::{Deserialize, Serialize, de, ser};
use url::Url;

use crate::quackpack::core::solver::gathering::fetch_types::{
    ManifestsRequest, NotPinnedRequest, PinnedRequest,
};
use crate::quackpack::core::solver::types_common::{InternedLocation, Location};
use crate::quackpack::core::version::CompatibilityCheck;
use crate::quackpack::core::{BranchOrTag, Dependency, Registry, Source, Version};
use crate::util::extract::Extract;
use crate::{QuackResult, QuackResultContext, StrId, qp_bail_internal};

static INTERNED_EXPANDED_LOCATION_CACHE: OnceLock<Mutex<HashSet<&'static ExpandedLocation>>> =
    OnceLock::new();

#[derive(Debug, Clone, Copy, Hash, PartialEq, Eq)]
/// Interned version of [`Location`].
pub struct InternedExpandedLocation {
    inner: &'static ExpandedLocation,
}

impl InternedExpandedLocation {
    pub fn new(source: ExpandedLocation) -> Self {
        let mut cache = INTERNED_EXPANDED_LOCATION_CACHE
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

impl ser::Serialize for InternedExpandedLocation {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: ser::Serializer,
    {
        self.deref().serialize(serializer)
    }
}

impl<'de> de::Deserialize<'de> for InternedExpandedLocation {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        let location = ExpandedLocation::deserialize(deserializer)?;
        Ok(Self::new(location))
    }
}

impl<T: Into<ExpandedLocation>> From<T> for InternedExpandedLocation {
    fn from(value: T) -> Self {
        Self::new(value.into())
    }
}

impl Deref for InternedExpandedLocation {
    type Target = ExpandedLocation;

    fn deref(&self) -> &'static Self::Target {
        self.inner
    }
}

impl AsRef<ExpandedLocation> for InternedExpandedLocation {
    fn as_ref(&self) -> &'static ExpandedLocation {
        self.inner
    }
}

#[derive(Clone, Deserialize, Eq, Hash, PartialEq, Serialize)]
/// Type describing a localization of a dependency.
/// Can either be:
/// * registry - a dependency with a given name from a given server;
/// * git - a dependency on a commit of a git repository at a given URL;
/// * local - a dependency on a package that is stored locally on disk.
///
/// Diffrence between [`Location`] and [`ExpandedLocation`]
/// -------------------------------------------------------
/// [`Location`] directly corresponds to an entry in manifest.
/// Specyfically, git dependencies can be specified by also tags or branches.
/// Thus different locations can actually specify the same package,
/// but it can only be known after cloning git repositories.
/// Thus firstly we use [`Location`] and after all the manifests are gathered,
/// we transition to using [`ExpandedLocation`].
pub enum ExpandedLocation {
    Registry { url: Url, real_name: StrId },
    Git { url: Url, commit: StrId },
    Local { absolute_path: PathBuf },
}

impl std::fmt::Debug for ExpandedLocation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Registry { url, real_name } => f
                .debug_struct("Registry")
                .field("url", &url.as_str())
                .field("real_name", real_name)
                .finish(),
            Self::Git { url, commit } => f
                .debug_struct("Git")
                .field("url", &url.as_str())
                .field("commit", commit)
                .finish(),
            Self::Local { absolute_path } => f
                .debug_struct("Local")
                .field("absolute_path", absolute_path)
                .finish(),
        }
    }
}

impl ExpandedLocation {
    pub fn is_registry(&self) -> bool {
        matches!(self, Self::Registry { .. })
    }

    pub fn is_git(&self) -> bool {
        matches!(self, Self::Git { .. })
    }

    pub fn is_local(&self) -> bool {
        matches!(self, Self::Local { .. })
    }

    /// Return a descriptive name of this location.
    pub fn descriptive_name(&self) -> String {
        match self {
            Self::Registry { real_name, .. } => format!("`{}", real_name),
            Self::Git { url, .. } => format!("cloned from `{url}`"),
            Self::Local { absolute_path } => {
                format!("at the directory `{}`", absolute_path.display())
            }
        }
    }
}

#[derive(Copy, Clone, Debug, Eq, Hash, PartialEq)]
/// Type describing a concrete package from the point of view of the solver.
/// This contains an [`ExpandedLocation`] and a version.
/// For git and local dependencies the version field is [`None`] and for registry
/// dependencies the version field contains the version of the dependency.
pub struct ExpandedPackage {
    pub location: InternedExpandedLocation,
    pub version: Option<Version>,
}

impl ExpandedPackage {
    pub fn location(&self) -> &ExpandedLocation {
        &self.location
    }

    pub fn version(&self) -> Option<Version> {
        self.version
    }

    pub fn is_compatible_with(&self, other: &ExpandedPackage) -> QuackResult<bool> {
        if !(self.location == other.location) {
            return Ok(false);
        }
        if self.location.is_registry() {
            let Some(version_self) = self.version else {
                qp_bail_internal!("Registry ExpandedPackage should have a version specified")
            };
            let Some(version_other) = other.version else {
                qp_bail_internal!("Registry ExpandedPackage should have a version specified")
            };
            Ok(version_other.can_be_upgraded_to(&version_self))
        } else {
            Ok(self.version == other.version)
        }
    }

    /// Assuming that [`self`] was a realization of some dependency, checks whether we can be certain it is still true.
    pub fn still_satisfies_dep(&self, dependency: &Dependency) -> QuackResult<bool> {
        match (self.location.as_ref(), dependency.source().as_ref()) {
            (ExpandedLocation::Local { absolute_path }, Source::Local(local_source)) => {
                Ok(absolute_path == local_source.absolute())
            }
            (ExpandedLocation::Git { url, commit }, Source::Git(git_source)) => {
                // If the git dependency specifies tag, branch or nothing (default branch),
                // some new commits may have appeared.
                if let Some(required_commit) = git_source.rev()
                    && *commit == required_commit
                    && url == git_source.url()
                {
                    if let Some(required_version) = dependency.versions().first() {
                        Ok(self.version == Some(*required_version))
                    } else {
                        Ok(true)
                    }
                } else {
                    Ok(false)
                }
            }
            (ExpandedLocation::Registry { url, real_name }, Source::Registry(registry_source)) => {
                self.check_satisfaction_for_registry(url, *real_name, registry_source, dependency)
            }
            _ => Ok(false),
        }
    }

    /// Helper for [`Self::still_satisfies_dep`].
    fn check_satisfaction_for_registry(
        &self,
        url: &Url,
        real_name: StrId,
        registry_source: &Registry,
        dependency: &Dependency,
    ) -> QuackResult<bool> {
        let location_agreement = (url == registry_source.url()) && (dependency.name() == real_name);
        let self_version = self
            .version
            .context_internal("Registry package with no version")?;
        if dependency.is_pinned() {
            let required_version = dependency
                .versions()
                .first()
                .context_internal("Pinned dependency without specified version")?;
            Ok(location_agreement && self_version == *required_version)
        } else {
            Ok(location_agreement
                && dependency
                    .versions()
                    .iter()
                    .any(|required| required.can_be_upgraded_to(&self_version)))
        }
    }

    /// Creates a [`ManifestsRequest`] for precisely that single package.
    pub fn create_manifest_request(&self) -> QuackResult<ManifestsRequest> {
        match self.location.as_ref() {
            ExpandedLocation::Registry { url, real_name } => {
                let version = self
                    .version
                    .context_internal("Registry package without version")?;
                Ok(ManifestsRequest::Pinned(PinnedRequest {
                    location: InternedLocation::new(Location::Registry {
                        url: url.clone(),
                        real_name: *real_name,
                    }),
                    version,
                    features: HashSet::new(),
                }))
            }
            ExpandedLocation::Git { url, commit } => {
                Ok(ManifestsRequest::NotPinned(NotPinnedRequest {
                    location: InternedLocation::new(Location::Git {
                        url: url.clone(),
                        branch_or_tag: BranchOrTag::Default,
                        rev: Some(*commit),
                    }),
                    versions: None,
                    features: HashSet::new(),
                }))
            }
            ExpandedLocation::Local { absolute_path } => {
                Ok(ManifestsRequest::NotPinned(NotPinnedRequest {
                    location: InternedLocation::new(Location::Local {
                        path: absolute_path.clone(),
                    }),
                    versions: None,
                    features: HashSet::new(),
                }))
            }
        }
    }
}
