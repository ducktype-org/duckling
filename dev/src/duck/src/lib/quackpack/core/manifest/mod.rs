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
mod venv_config;

pub use dependency::*;
pub use features::*;
pub use metadata::*;
pub use parse::*;
pub use profiles::*;
pub use source::*;
pub use venv_config::*;

use super::valid_package_name::validate_package_name;
use crate::duck::util::duck_home::DuckHome;
use crate::quackpack::core::Version;
use crate::quackpack::schemas::registry;
use crate::{DuckContext, QuackError, QuackResultContext, StrId};

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
    venv: VenvConfig,
}

impl Manifest {
    /// Create a new [`Manifest`].
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        name: StrId,
        version: Version,
        features: Features,
        metadata: PackageMetadata,
        dependencies: Dependencies,
        dev_dependencies: Dependencies,
        profiles: Profiles,
        venv: VenvConfig,
    ) -> Self {
        Self {
            name,
            version,
            features,
            metadata,
            dependencies,
            dev_dependencies,
            profiles,
            venv,
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

    /// Check if there are any local dependencies.
    pub fn has_local_deps(&self) -> bool {
        self.dependencies()
            .all_dependencies()
            .iter()
            .any(|dep| dep.source().is_local())
    }

    /// Get the venv configuration.
    pub fn venv(&self) -> &VenvConfig {
        &self.venv
    }
}

impl TryFrom<(registry::Manifest, &DuckContext)> for Manifest {
    type Error = QuackError;

    fn try_from((value, ctx): (registry::Manifest, &DuckContext)) -> Result<Self, Self::Error> {
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
        let metadata = PackageMetadata {
            authors,
            license: Some(license),
            description: Some(description),
        };
        validate_package_name(&name)
            .context("registry responded with a package with an invalid name")?;
        Ok(Manifest::new(
            name.into(),
            version,
            features.try_into()?,
            metadata,
            dependencies.try_into()?,
            dev_dependencies.try_into()?,
            profiles.into(),
            VenvConfig::default_for_package(ctx),
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
            venv: _,
        } = value;
        let license = metadata.license.unwrap_or_default();
        let description = metadata.description.unwrap_or_default();
        let authors = metadata.authors.into_iter().collect();
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
