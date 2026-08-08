use crate::{DuckContext, QuackResult, QuackResultContext};
use crate::quackpack::core::{AllowGlobalPackage, PackageLoader};

#[derive(Debug, Default, Clone)]
/// All options that can be passed to sync.
pub struct RemoveOptions {
    /// Name of the dependency
    pub name: String,
    /// Use a global package instead of a local one.
    pub global: bool,
    /// Remove a dev-dependency.
    pub dev_dep: bool,
}

pub fn add(ctx: &DuckContext, options: RemoveOptions) -> QuackResult<()> {
    let RemoveOptions { name, global, dev_dep } = options;
    let mut pkg = if global {
        PackageLoader::global_package(ctx)?
    } else {
        PackageLoader::find_from_cwd(ctx, AllowGlobalPackage::No)?
    }.into_package().unwrap_package();
    let dependencies_map = if dev_dep {
        pkg.schema_mut().dev_dependencies_mut()
    } else {
        pkg.schema_mut().dependencies_mut()
    };
    let _ = dependencies_map.map(|map| map.remove_entry(&name)).flatten().with_context(|| format!("no such dependency as {name}")
    )?;
    Ok(())
}