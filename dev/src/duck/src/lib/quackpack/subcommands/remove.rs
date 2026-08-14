use std::path::Path;

use crate::quackpack::core::{AllowGlobalPackage, PackageLoader};
use crate::quackpack::schemas::manifest::{DependencyRemoved, Manifest as ManifestSchema};
use crate::util::path_ops_ext::PathOpsExt;
use crate::{
    DuckContext, QuackError, QuackResult, QuackResultContext, StrId, qp_bail, qp_bail_internal,
};

#[derive(Debug, Clone)]
/// All options that can be passed to remove.
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
    if dev_dep {
        remove_dev_dep(&mut schema, &name, pkg_name, &pkg_root)?;
    } else {
        remove_normal_dep(&mut schema, &name, pkg_name, &pkg_root)?;
    }
    let deserialized_schema = serde_yaml_ng::to_string(&schema)
        .with_context_internal(|| format!("failed to deserialize schema `{schema:?}`"))?;
    manifest_path.write(&deserialized_schema).with_context(|| {
        format!(
            "failed to write the new manifest into file at `{}`",
            manifest_path.display()
        )
    })?;
    ctx.console().info(format!(
        "written new manifest to `{}`",
        manifest_path.display()
    ))?;
    ctx.console().info(format!(
        "successfully removed {}dependency `{name}` from the project `{pkg_name}` at `{}`",
        if dev_dep { "dev-" } else { "" },
        pkg_root.display(),
    ))?;
    Ok(())
}

/// Remove a dev-dependency or provide a meaningful error.
fn remove_dev_dep(
    schema: &mut ManifestSchema,
    name: &String,
    pkg_name: StrId,
    pkg_root: &Path,
) -> QuackResult<()> {
    match schema.remove_dev_dependency(name) {
        DependencyRemoved::Yes => Ok(()),
        DependencyRemoved::NoDependency => qp_bail!(
            "no such dev-dependency as `{name}` in the project `{pkg_name}` at `{}`",
            pkg_root.display()
        ),
        DependencyRemoved::NoDependencyButDevDepExists => {
            qp_bail_internal!("there exists dev-dependency `{name}` but we did not remove it")
        }
    }
}

/// Remove a dependency or provide a meaningful error.
fn remove_normal_dep(
    schema: &mut ManifestSchema,
    name: &String,
    pkg_name: StrId,
    pkg_root: &Path,
) -> QuackResult<()> {
    match schema.remove_dependency(name) {
        DependencyRemoved::Yes => Ok(()),
        DependencyRemoved::NoDependency => qp_bail!(
            "no such dependency as `{name}` in the project `{pkg_name}` at `{}`",
            pkg_root.display()
        ),
        DependencyRemoved::NoDependencyButDevDepExists => {
            let err = Err(QuackError::hint(
                "if you want to remove a dev-dependency use flag `--dev`",
            ));
            err.with_context(|| {
                format!(
                    "no such dependency as `{name}` in the project `{pkg_name}` at `{}`",
                    pkg_root.display()
                )
            })
        }
    }
}
