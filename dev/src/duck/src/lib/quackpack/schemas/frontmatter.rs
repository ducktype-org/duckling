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
    /// `import:` root field
    pub import: Option<PathBuf>,
}
