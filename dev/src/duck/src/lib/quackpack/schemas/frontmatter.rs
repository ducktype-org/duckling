use std::{collections::HashMap, path::PathBuf};

use serde::Deserialize;

use crate::quackpack::schemas::manifest::{Dependencies, Profile};

#[derive(Debug, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub struct FrontMatter {
    /// `dependencies:` root field
    pub dependencies: Option<Dependencies>,
    /// `dev_dependencies:` root field
    pub dev_dependencies: Option<Dependencies>,
    /// `profiles:` root field
    pub profiles: Option<HashMap<String, Profile>>,
    /// `import:` root field, relative to the parent folder of the script
    pub import: Option<PathBuf>,
}

impl FrontMatter {
    /// Checks whether at most the [`FrontMatter::import`] field is not [`None`].
    pub fn is_just_import_or_empty(&self) -> bool {
        self.dependencies.is_none() && self.dev_dependencies.is_none() && self.profiles.is_none()
    }
}
