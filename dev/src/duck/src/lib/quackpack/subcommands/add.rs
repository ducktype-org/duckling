use crate::{DuckContext, QuackResult};
use crate::quackpack::core::{AllowGlobalPackage, FeatureName, PackageLoader};

#[derive(Debug, Default, Clone)]
/// All options that can be passed to sync.
pub struct AddOptions {
    /// Name of the dependency
    /// Use a global package instead of a local one.
    pub global: bool,
    pub features: Vec<FeatureName>,
}

pub fn add(ctx: &DuckContext, options: AddOptions) -> QuackResult<()> {
    let AddOptions { global, features } = options;
    let pkg = if options.global {
        PackageLoader::global_package(ctx)?
    } else {
        PackageLoader::find_from_cwd(ctx, AllowGlobalPackage::No)?
    };
    
}