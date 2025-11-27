//! High-level abstraction over a manifest and its inner types.
//!
//! The most notable members are [`Manifest`], [`Dependency`],
//! [`DependencyDescription`] and [`RootDescription`].
//!
//! Parsing is implemented in the [`parse`] module.
mod compiler_options;
mod dependency;
mod features;
mod parse;
mod root_description;
mod source;

pub use parse::*;

pub use compiler_options::*;
pub use dependency::*;
pub use features::*;
pub use root_description::*;
pub use source::*;

use crate::StrId;

#[derive(Debug)]
/// Machine friendly abstraction over a manifest.
pub struct Manifest {
    root_description: RootDescription,
    features: Features,
    authors: Vec<StrId>,
    license: Option<StrId>,
    description: Option<StrId>,
    dependencies: Dependencies,
    dev_dependencies: Dependencies,
    profiles: Profiles,
}

impl Manifest {
    /// Create a new manifest.
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        root_description: RootDescription,
        features: Features,
        authors: Vec<StrId>,
        license: Option<StrId>,
        description: Option<StrId>,
        dependencies: Dependencies,
        dev_dependencies: Dependencies,
        profiles: Profiles,
    ) -> Self {
        Self {
            root_description,
            features,
            authors,
            license,
            description,
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

    /// Get the list of authors.
    pub fn authors(&self) -> &[StrId] {
        &self.authors
    }

    /// Get the license identifier, if any.
    pub fn license(&self) -> Option<StrId> {
        self.license
    }

    /// Get the package description, if any.
    pub fn description(&self) -> Option<StrId> {
        self.description
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
