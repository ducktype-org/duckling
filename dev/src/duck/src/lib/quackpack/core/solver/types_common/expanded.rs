use std::{
    collections::HashSet,
    ops::Deref,
    path::PathBuf,
    sync::{Mutex, OnceLock},
};

use serde::{Deserialize, Serialize, de, ser};
use url::Url;

use crate::{
    QuackResult, QuackResultContext, StrId, qp_bail_internal,
    quackpack::core::{Dependency, Registry, Source, Version, version::CompatibilityCheck},
};

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
            // NOTE: `.unwrap()` should never panic: from docs:
            // Errors
            //
            // If another user of this mutex panicked while holding the mutex,
            // then this call will return an error once the mutex is acquired.
            // The acquired mutex guard will be contained in the returned error.
            //
            // Panics
            //
            // This function might panic when called if the lock is already held by the current thread.
            .unwrap();
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

impl From<ExpandedLocation> for InternedExpandedLocation {
    fn from(value: ExpandedLocation) -> Self {
        Self::new(value)
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

#[derive(Clone, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum ExpandedLocation {
    Registry(ExpandedLocRegistry),
    Git(ExpandedLocGit),
    Local(ExpandedLocLocal),
}

#[derive(Clone, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ExpandedLocRegistry {
    pub url: Url,
    pub real_name: StrId,
}

#[derive(Clone, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ExpandedLocGit {
    pub url: Url,
    pub commit: StrId,
}

#[derive(Clone, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ExpandedLocLocal {
    pub absolute_path: PathBuf,
}

impl ExpandedLocation {
    pub fn is_registry(&self) -> bool {
        matches!(self, Self::Registry(_))
    }

    pub fn is_git(&self) -> bool {
        matches!(self, Self::Git(_))
    }

    pub fn is_local(&self) -> bool {
        matches!(self, Self::Local(_))
    }
}

#[derive(Copy, Clone, Debug, Eq, Hash, PartialEq)]
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
        match (self.location.as_ref(), dependency.desc().source().as_ref()) {
            (ExpandedLocation::Local(local_loc), Source::Local(local_source)) => {
                Ok(local_loc.absolute_path == local_source.absolute())
            }
            (ExpandedLocation::Git(git_loc), Source::Git(git_source)) => {
                // If the git dependency specifies tag, branch or nothing (default branch),
                // some new commits may have appeared.
                if let Some(required_commit) = git_source.rev()
                    && git_loc.commit == required_commit
                    && git_loc.url == *git_source.url()
                {
                    if let Some(required_version) = dependency.desc().versions().first() {
                        Ok(self.version == Some(*required_version))
                    } else {
                        Ok(true)
                    }
                } else {
                    Ok(false)
                }
            }
            (ExpandedLocation::Registry(registry_loc), Source::Registry(registry_source)) => {
                self.check_satisfaction_for_registry(registry_loc, registry_source, dependency)
            }
            _ => Ok(false),
        }
    }

    /// Helper for [`Self::still_satisfies_dep`].
    fn check_satisfaction_for_registry(
        &self,
        registry_loc: &ExpandedLocRegistry,
        registry_source: &Registry,
        dependency: &Dependency,
    ) -> QuackResult<bool> {
        let location_agreement = (registry_loc.url == *registry_source.url())
            && (dependency.real_name() == registry_loc.real_name);
        let self_version = self
            .version
            .context_internal("Registry package with no version")?;
        if dependency.is_pinned() {
            let required_version = dependency
                .desc()
                .versions()
                .first()
                .context_internal("Pinned dependency without specified version")?;
            Ok(location_agreement && self_version == *required_version)
        } else {
            Ok(location_agreement
                && dependency
                    .desc()
                    .versions()
                    .iter()
                    .any(|required| required.can_be_upgraded_to(&self_version)))
        }
    }
}
