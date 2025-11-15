use crate::{
    StrId,
    quackpack::core::{Dependencies, Features, Profiles, RootSpec, Targets},
};

#[derive(Debug)]
pub struct Summary {
    spec: RootSpec,
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
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        spec: RootSpec,
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
            spec,
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

    pub fn spec(&self) -> &RootSpec {
        &self.spec
    }

    pub fn features(&self) -> &Features {
        &self.features
    }

    pub fn authors(&self) -> &[StrId] {
        &self.authors
    }

    pub fn license(&self) -> Option<StrId> {
        self.license
    }

    pub fn description(&self) -> Option<StrId> {
        self.description
    }

    pub fn dependencies(&self) -> &Dependencies {
        &self.dependencies
    }

    pub fn dev_dependencies(&self) -> &Dependencies {
        &self.dev_dependencies
    }

    pub fn profiles(&self) -> &Profiles {
        &self.profiles
    }

    pub fn targets(&self) -> &Targets {
        &self.targets
    }
}
