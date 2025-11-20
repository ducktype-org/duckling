use crate::{
    StrId,
    quackpack::core::{Dependencies, Features, Profiles, RootDescription, Targets},
};

#[derive(Debug)]
/// High-level summary of a parsed manifest.
pub struct Summary {
    desc: RootDescription,
    features: Features,
    authors: Vec<StrId>,
    license: Option<StrId>,
    description: Option<StrId>,
    dependencies: Dependencies,
    dev_dependencies: Dependencies,
    profiles: Profiles,
    targets: Targets,
}

impl Summary {
    /// Create a new summary.
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        desc: RootDescription,
        features: Features,
        authors: Vec<StrId>,
        license: Option<StrId>,
        description: Option<StrId>,
        dependencies: Dependencies,
        dev_dependencies: Dependencies,
        profiles: Profiles,
        targets: Targets,
    ) -> Self {
        Self {
            desc,
            features,
            authors,
            license,
            description,
            dependencies,
            dev_dependencies,
            profiles,
            targets,
        }
    }

    /// Get the root package description.
    pub fn desc(&self) -> &RootDescription {
        &self.desc
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

    /// Get the compiler specific options for target.
    pub fn targets(&self) -> &Targets {
        &self.targets
    }
}
