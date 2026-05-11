use std::collections::{HashMap, HashSet};
use std::hash::Hash;
use std::ops::Deref;
use std::path::PathBuf;
use std::sync::{Mutex, OnceLock};

use url::Url;

use crate::quackpack::core::solver::types_common::expanded::InternedExpandedLocation;
use crate::quackpack::core::solver::types_common::{ExpandedLocation, ExpandedPackage};
use crate::quackpack::core::version::CompatibilityCheck;
use crate::quackpack::core::{BranchOrTag, Dependency, Source, Version};
use crate::util::extract::Extract;
use crate::{QuackResult, StrId, qp_bail_internal};

static INTERNED_LOCATION_CACHE: OnceLock<Mutex<HashSet<&'static Location>>> = OnceLock::new();

#[derive(Debug, Clone, Copy)]
/// Interned version of [`Location`].
pub struct InternedLocation {
    inner: &'static Location,
}

impl InternedLocation {
    pub fn new(source: Location) -> Self {
        let mut cache = INTERNED_LOCATION_CACHE
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

impl<T: Into<Location>> From<T> for InternedLocation {
    fn from(value: T) -> Self {
        Self::new(value.into())
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

impl PartialEq for InternedLocation {
    fn eq(&self, other: &Self) -> bool {
        std::ptr::eq(self.inner, other.inner)
    }
}

impl Eq for InternedLocation {}

impl Hash for InternedLocation {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        (self.inner as *const Location as usize).hash(state);
    }
}

#[derive(Clone, Eq, Hash, PartialEq)]
pub enum Location {
    Registry {
        url: Url,
        real_name: StrId,
    },
    Git {
        url: Url,
        branch_or_tag: BranchOrTag,
        rev: Option<StrId>,
    },
    Local {
        path: PathBuf,
    },
}

impl std::fmt::Debug for Location {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Registry { url, real_name } => f
                .debug_struct("Registry")
                .field("url", &url.as_str())
                .field("real_name", real_name)
                .finish(),
            Self::Git {
                url,
                branch_or_tag,
                rev,
            } => f
                .debug_struct("Git")
                .field("url", &url.as_str())
                .field("branch_or_tag", branch_or_tag)
                .field("rev", rev)
                .finish(),
            Self::Local { path } => f.debug_struct("Local").field("path", path).finish(),
        }
    }
}

impl From<&Dependency> for Location {
    fn from(dependency: &Dependency) -> Self {
        match &dependency.source().as_ref() {
            Source::Registry(registry) => Self::Registry {
                url: registry.url().clone(),
                real_name: dependency.name(),
            },
            Source::Local(local) => Self::Local {
                path: local.absolute().to_path_buf(),
            },
            Source::Git(git) => Self::Git {
                url: git.url().clone(),
                branch_or_tag: git.branch_or_tag().clone(),
                rev: git.rev(),
            },
        }
    }
}

impl Location {
    pub fn is_registry(&self) -> bool {
        matches!(self, Self::Registry { .. })
    }

    pub fn is_git(&self) -> bool {
        matches!(self, Self::Git { .. })
    }

    pub fn is_local(&self) -> bool {
        matches!(self, Self::Local { .. })
    }

    pub fn canonical_unexpansion(expanded_loc: &ExpandedLocation) -> Self {
        match expanded_loc {
            ExpandedLocation::Registry { url, real_name } => Self::Registry {
                url: url.clone(),
                real_name: *real_name,
            },
            ExpandedLocation::Git { url, commit } => Self::Git {
                url: url.clone(),
                branch_or_tag: BranchOrTag::Default,
                rev: Some(*commit),
            },
            ExpandedLocation::Local { absolute_path } => Self::Local {
                path: absolute_path.clone(),
            },
        }
    }
}

#[derive(Copy, Clone, Debug, Eq, Hash, PartialEq)]
/// Type describing a package from the point of the gathering manifests process.
/// This contains a [`Location`] and a version.
/// For git and local dependencies the version field is [`None`] and for registry
/// dependencies the version field contains the version of the dependency.
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

    /// Create the appropriate [`ExpandedPackage`] from this [`Package`].
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
