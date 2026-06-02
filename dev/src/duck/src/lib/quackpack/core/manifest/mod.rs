//! High-level abstraction over a manifest and its inner types.
//!
//! The most notable members are [`Manifest`] and [`Dependency`].
//!
//! Parsing is implemented in the [`parse`] module.
mod dependency;
mod features;
mod metadata;
mod parse;
mod profiles;
mod source;

pub use dependency::*;
pub use features::*;
pub use metadata::*;
pub use parse::*;
pub use profiles::*;
pub use source::*;

use crate::duck::util::duck_home::DuckHome;
use crate::quackpack::core::Version;
use crate::quackpack::schemas::registry;
use crate::{QuackError, StrId};

#[derive(Clone, Debug)]
/// Machine friendly abstraction over a manifest.
pub struct Manifest {
    name: StrId,
    version: Version,
    features: Features,
    metadata: PackageMetadata,
    dependencies: Dependencies,
    dev_dependencies: Dependencies,
    profiles: Profiles,
}

impl Manifest {
    /// Create a new [`Manifest`].
    pub fn new(
        name: StrId,
        version: Version,
        features: Features,
        metadata: PackageMetadata,
        dependencies: Dependencies,
        dev_dependencies: Dependencies,
        profiles: Profiles,
    ) -> Self {
        Self {
            name,
            version,
            features,
            metadata,
            dependencies,
            dev_dependencies,
            profiles,
        }
    }

    /// Get the package name.
    pub fn name(&self) -> StrId {
        self.name
    }

    /// Get the package version.
    pub fn version(&self) -> Version {
        self.version
    }

    /// Get the root package exposed features.
    pub fn features(&self) -> &Features {
        &self.features
    }

    /// Get the package metadata
    pub fn metadata(&self) -> &PackageMetadata {
        &self.metadata
    }

    /// Get the dependencies.
    pub fn dependencies(&self) -> &Dependencies {
        &self.dependencies
    }

    /// Get mutable access to the dependencies.
    pub fn dependencies_mut(&mut self) -> &mut Dependencies {
        &mut self.dependencies
    }

    /// Get the development dependencies.
    pub fn dev_dependencies(&self) -> &Dependencies {
        &self.dev_dependencies
    }

    /// Get the compiler specific options for profile.
    pub fn profiles(&self) -> &Profiles {
        &self.profiles
    }

    /// Check if this is the manifest of the global venv.
    pub fn is_global(&self) -> bool {
        self.name == DuckHome::GLOBAL_PACKAGE_NAME
    }
}

impl TryFrom<registry::Manifest> for Manifest {
    type Error = QuackError;

    fn try_from(value: registry::Manifest) -> Result<Self, Self::Error> {
        let registry::Manifest {
            metadata,
            dependencies,
            dev_dependencies,
            features,
            profiles,
        } = value;
        let registry::Metadata {
            version,
            authors,
            license,
            name,
            description,
        } = metadata;
        let authors = authors.into_iter().collect();
        let metadata = PackageMetadata::new(authors, Some(license), Some(description));
        Ok(Manifest::new(
            name.into(),
            version,
            features.try_into()?,
            metadata,
            dependencies.try_into()?,
            dev_dependencies.try_into()?,
            profiles.into(),
        ))
    }
}

impl TryFrom<Manifest> for registry::Manifest {
    type Error = QuackError;

    fn try_from(value: Manifest) -> Result<Self, Self::Error> {
        let Manifest {
            name,
            version,
            features,
            metadata,
            dependencies,
            dev_dependencies,
            profiles,
        } = value;
        let license = metadata.license().map(Into::into).unwrap_or_default();
        let description = metadata.description().map(Into::into).unwrap_or_default();
        let authors = metadata.into_authors().into_iter().collect();
        let metadata = registry::Metadata {
            version,
            authors,
            license,
            name: name.into(),
            description,
        };
        Ok(Self {
            metadata,
            dependencies: dependencies.try_into()?,
            dev_dependencies: dev_dependencies.try_into()?,
            features: features.into(),
            profiles: profiles.into(),
        })
    }
}
