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

    // Only necessary for diagnostic messages.
    let pkg_name = pkg.name();
    let pkg_root = pkg.root_directory().to_path_buf();

    // @TODO: #1394 We would like to use a better mechanism than modify deserialized schema -> blindly serialize it,
    // since this won't preserve comments and formatting choices in the manifest.
    let manifest_path = pkg.manifest_path().to_path_buf();
    let mut schema = pkg.into_original_schema();
    let dependencies_map = if dev_dep {
        schema.dev_dependencies_mut()
    } else {
        schema.dependencies_mut()
    };
    let _ = dependencies_map
        .and_then(|map| map.remove_entry(&name))
        .with_context(|| format!("no such dependency as `{name}` in project `{pkg_name}` at `{}`", pkg_root.display()))?;
    ctx.console().info(format!("removed {}dependency `{name}` from project `{pkg_name}` at `{}`", if dev_dep { "dev-" } else { "" }, pkg_root.display()))?;
    let deserialized_schema = serde_yaml_ng::to_string(&schema)?;
    manifest_path.write(deserialized_schema)?;
    Ok(())
}
