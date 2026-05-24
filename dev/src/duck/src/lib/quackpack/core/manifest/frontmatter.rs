use crate::quackpack::core::{Dependencies, Profiles};

#[derive(Clone, Debug)]
/// Machine friendly abstraction over a frontmatter.
pub struct FrontMatter {
    dependencies: Dependencies,
    dev_dependencies: Dependencies,
    profiles: Profiles,
}

impl FrontMatter {
    /// Create a new [`FrontMatter`].
    pub fn new(
        dependencies: Dependencies,
        dev_dependencies: Dependencies,
        profiles: Profiles,
    ) -> Self {
        Self {
            dependencies,
            dev_dependencies,
            profiles,
        }
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
}
