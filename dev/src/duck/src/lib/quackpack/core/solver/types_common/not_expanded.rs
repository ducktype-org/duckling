use std::{
    collections::{HashMap, HashSet},
    ops::Deref,
    sync::{Mutex, OnceLock},
};

use url::Url;

use crate::{
    QuackResult, StrId, qp_bail_internal,
    quackpack::core::{
        BranchOrTag, Dependency, Source, Version,
        types_common::{ExpandedPackage, expanded::InternedExpandedLocation},
        version::CompatibilityCheck,
    },
};

static INTERNED_LOCATION_CACHE: OnceLock<Mutex<HashSet<&'static Location>>> = OnceLock::new();

#[derive(Debug, Clone, Copy, Hash, PartialEq, Eq)]
/// Interned version of [`Location`].
pub struct InternedLocation {
    inner: &'static Location,
}

impl InternedLocation {
    pub fn new(source: Location) -> Self {
        let mut cache = INTERNED_LOCATION_CACHE
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

impl From<Location> for InternedLocation {
    fn from(value: Location) -> Self {
        Self::new(value)
    }
}

impl Deref for InternedLocation {
    type Target = Location;

    fn deref(&self) -> &'static Self::Target {
        self.inner
    }
}

impl AsRef<Location> for InternedLocation {
    fn as_ref(&self) -> &'static Location {
        self.inner
    }
}

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub enum Location {
    Registry(LocRegistry),
    Git(LocGit),
    Local(LocLocal),
}

impl From<&Dependency> for Location {
    fn from(dependency: &Dependency) -> Self {
        match &dependency.desc().source().as_ref() {
            Source::Registry(registry) => Self::Registry(LocRegistry {
                url: registry.url().clone(),
                real_name: dependency.real_name(),
            }),
            Source::Local(local) => Self::Local(LocLocal {
                path: local.entry_in_manifest(),
            }),
            Source::Git(git) => Self::Git(LocGit {
                url: git.url().clone(),
                branch_or_tag: git.branch_or_tag(),
                rev: git.rev(),
            }),
        }
    }
}

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct LocRegistry {
    pub url: Url,
    pub real_name: StrId,
}

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct LocGit {
    pub url: Url,
    pub branch_or_tag: BranchOrTag,
    pub rev: Option<StrId>,
}

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct LocLocal {
    pub path: StrId,
}

impl Location {
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
pub struct Package {
    pub location: InternedLocation,
    pub version: Option<Version>,
}

impl Package {
    pub fn location(&self) -> &Location {
        &self.location
    }

    pub fn version(&self) -> Option<Version> {
        self.version
    }

    pub fn is_compatible_with(&self, other: &Package) -> QuackResult<bool> {
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

    pub fn resolve(
        self,
        location_resolver: &HashMap<InternedLocation, InternedExpandedLocation>,
    ) -> Option<ExpandedPackage> {
        location_resolver
            .get(&self.location)
            .map(|loc| ExpandedPackage {
                location: *loc,
                version: self.version,
            })
    }
}
