use crate::quackpack::core::{AllowGlobalPackage, PackageLoader};
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult, QuackResultContext};

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

/// Logic for executing the `remove` subcommand.
pub fn remove(ctx: &DuckContext, options: RemoveOptions) -> QuackResult<()> {
    let RemoveOptions {
        name,
        global,
        dev_dep,
    } = options;
    let pkg = if global {
        PackageLoader::global_package(ctx)?
    } else {
        PackageLoader::find_from_cwd(ctx, AllowGlobalPackage::No)?
    }
    .into_package()
    .unwrap_package();
    let manifest_path = pkg.manifest_path().to_path_buf();
    let mut schema = pkg.into_original_schema();
    let dependencies_map = if dev_dep {
        schema.dev_dependencies_mut()
    } else {
        schema.dependencies_mut()
    };
    ctx.console().info(format!("removing {}dependency {}", if dev_dep { "dev-" } else { "" }, name))?;
    let _ = dependencies_map
        .and_then(|map| map.remove_entry(&name))
        .with_context(|| format!("no such dependency as {name}"))?;
    let deserialized_schema = serde_yaml_ng::to_string(&schema)?;
    manifest_path.write(deserialized_schema)?;
    Ok(())
}
