use crate::{DuckContext, QuackResult};
use crate::quackpack::core::Version;
use crate::quackpack::schemas::manifest::{Dependency, DependencyFeature, DependencySource, OredSemver};

pub struct AddOptions {
    /// Name of the dependency to add.
    pub name: String,
    /// Use a global package instead of a local one.
    pub global: bool,
    /// Add a dev-dependency.
    pub dev_dep: bool,
    /// Source of the dependency.
    pub source: Option<DependencySource>,
    /// How this dependency should be aliased.
    pub alias: Option<String>,
    /// Required versions of this dependency.
    pub versions: Vec<Version>,
    /// Features of the dependency.
    pub features: Vec<String>,
    /// Whether the dependency should be pinned.
    pub pinned: bool,
}

/// Logic for executing the `add` subcommand.
pub fn add(ctx: &DuckContext, options: AddOptions) -> QuackResult<()> {
    let AddOptions { name, global, dev_dep, source, alias, versions, features, pinned } = options;
    // For now it is not possible to specify feature conditions through `add` interface.
    let features: Vec<DependencyFeature> = features.into_iter().map(|f| DependencyFeature::Simple(f)).collect();
    // For now it is not possible to specify dependency conditions through `add` interface.
    let dep = Dependency {
        version: if versions.is_empty() { None } else { Some(OredSemver(versions)) },
        source,
        features: if features.is_empty() { None } else { Some(features) },
        pinned: if pinned { Some(true) } else { None },
        conditions: None,
    };
    let alias = alias.unwrap_or(name);
}
