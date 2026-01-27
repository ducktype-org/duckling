use std::{
    collections::HashSet,
    ops::Deref,
    path::PathBuf,
    sync::{Mutex, OnceLock},
};

use url::Url;

use crate::{
    QuackResult, StrId, qp_bail_internal,
    quackpack::core::{Version, version::CompatibilityCheck},
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

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub enum ExpandedLocation {
    Registry(ExpandedLocRegistry),
    Git(ExpandedLocGit),
    Local(ExpandedLocLocal),
}

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct ExpandedLocRegistry {
    pub url: Url,
    pub real_name: StrId,
}

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct ExpandedLocGit {
    pub url: StrId,
    pub commit: StrId,
}

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
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
}
