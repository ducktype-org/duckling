use std::path::PathBuf;

use crate::{
    QuackResult, StrId, qp_bail_internal,
    quackpack::core::{Version, version::CompatibilityCheck},
};

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub enum ExpandedLocation {
    Registry(ExpandedLocRegistry),
    Git(ExpandedLocGit),
    Local(ExpandedLocLocal),
}

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct ExpandedLocRegistry {
    pub url: StrId,
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

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct ExpandedPackage {
    pub location: ExpandedLocation,
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
