//! High-level abstraction over a manifest and its inner types.
//!
//! The most notable members are [`Manifest`], [`Dependency`],
//! [`DependencyDescription`] and [`RootDescription`].
//!
//! Parsing is implemented in the [`parse`] module.
mod compiler_options;
mod dependency;
mod features;
mod metadata;
mod parse;
mod root_description;
mod source;

pub use parse::*;

pub use compiler_options::*;
pub use dependency::*;
pub use features::*;
pub use metadata::*;
pub use root_description::*;
pub use source::*;

use crate::{QuackError, quackpack::schemas::registry};

#[derive(Debug)]
/// Machine friendly abstraction over a manifest.
pub struct Manifest {
    root_description: RootDescription,
    features: Features,
    metadata: PackageMetadata,
    dependencies: Dependencies,
    dev_dependencies: Dependencies,
    profiles: Profiles,
}

impl Manifest {
    /// Create a new manifest.
    pub fn new(
        root_description: RootDescription,
        features: Features,
        metadata: PackageMetadata,
        dependencies: Dependencies,
        dev_dependencies: Dependencies,
        profiles: Profiles,
    ) -> Self {
        Self {
            root_description,
            features,
            metadata,
            dependencies,
            dev_dependencies,
            profiles,
        }
    }

    /// Get the root package description.
    pub fn root_description(&self) -> &RootDescription {
        &self.root_description
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

    /// Get the development dependencies.
    pub fn dev_dependencies(&self) -> &Dependencies {
        &self.dev_dependencies
    }

    /// Get the compiler specific options for profile.
    pub fn profiles(&self) -> &Profiles {
        &self.profiles
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
        let root_description = RootDescription::new(name.into(), version);
        let authors = authors.into_iter().map(Into::into).collect();
        let metadata =
            PackageMetadata::new(authors, Some(license.into()), Some(description.into()));
        Ok(Manifest::new(
            root_description,
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
            root_description,
            features,
            metadata,
            dependencies,
            dev_dependencies,
            profiles,
        } = value;
        let license = metadata.license().map(Into::into).unwrap_or_default();
        let description = metadata.description().map(Into::into).unwrap_or_default();
        let authors = metadata
            .into_authors()
            .into_iter()
            .map(Into::into)
            .collect();
        let metadata = registry::Metadata {
            version: root_description.version(),
            authors,
            license,
            name: root_description.name().into(),
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
