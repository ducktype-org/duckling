use std::collections::HashMap;

use crate::{
    QuackResult, StrId, qp_bail_internal,
    quackpack::core::{
        Dependency, GitRevision, Source, Version,
        types_common::{ExpandedLocation, ExpandedPackage},
        version::CompatibilityCheck,
    },
};

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub enum Location {
    Registry(LocRegistry),
    Git(LocGit),
    Local(LocLocal),
}

impl From<&Dependency> for Location {
    fn from(dependency: &Dependency) -> Self {
        match &dependency.desc().source() {
            Source::Registry(registry) => Self::Registry(LocRegistry {
                url: registry.url(),
                real_name: dependency.real_name(),
            }),
            Source::Local(local) => Self::Local(LocLocal {
                path: local.entry_in_manifest(),
            }),
            Source::Git(git) => Self::Git(LocGit {
                url: git.url(),
                rev: git.rev(),
                commit: git.commit(),
            }),
        }
    }
}

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct LocRegistry {
    pub url: StrId,
    pub real_name: StrId,
}

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct LocGit {
    pub url: StrId,
    pub rev: GitRevision,
    pub commit: Option<StrId>,
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

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct Package {
    pub location: Location,
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
        location_resolver: &HashMap<Location, ExpandedLocation>,
    ) -> Option<ExpandedPackage> {
        match location_resolver.get(&self.location) {
            None => None,
            Some(loc) => Some(ExpandedPackage {
                location: loc.clone(),
                version: self.version,
            }),
        }
    }
}
