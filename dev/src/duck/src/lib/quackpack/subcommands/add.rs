use crate::{DuckContext, QuackResult, StrId};
use crate::quackpack::core::{FeatureName, Source, Version};

pub struct AddOptions {
    /// Name of the dependency to add.
    pub name: StrId,
    /// Use a global package instead of a local one.
    pub global: bool,
    /// Add a dev-dependency.
    pub dev_dep: bool,
    /// Source of the dependency.
    pub source: Source,
    /// How this dependency should be aliased.
    pub alias: Option<StrId>,
    /// Required versions of this dependency.
    pub versions: Vec<Version>,
    /// Features of the dependency.
    pub features: Vec<FeatureName>,
    /// Whether the dependency should be pinned.
    pub pinned: bool,
}

pub fn add(ctx: &DuckContext, options: AddOptions) -> QuackResult<()> {
    Ok(())
}
