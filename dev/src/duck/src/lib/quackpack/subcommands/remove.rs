use std::path::Path;

use crate::quackpack::core::{
    AllowGlobalPackage, DependencyKind, DependencyRemoved, EditableManifest, PackageLoader,
};
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext, StrId, qp_bail};

#[derive(Debug, Clone)]
/// All options that can be passed to remove.
pub struct RemoveOptions<'matches> {
    /// Name of the dependency
    pub name: &'matches str,
    /// Use a global package instead of a local one.
    pub global: bool,
    /// Kind of the dependency to remove.
    pub kind: DependencyKind,
}

/// Logic for executing the `remove` subcommand.
pub fn remove(ctx: &DuckContext, options: RemoveOptions) -> QuackResult<()> {
    let RemoveOptions { name, global, kind } = options;
    let pcx = if global {
        PackageLoader::global_package(ctx)?
    } else {
        PackageLoader::find_from_cwd(ctx, AllowGlobalPackage::No)?
    };
    let pkg = pcx.package().get_package();

    // Only necessary for diagnostic messages.
    let pkg_name = pkg.name();
    let pkg_root = pkg.root_directory().to_path_buf();

    let editable_manifest = EditableManifest::new(&pcx)?;

    remove_dep(&editable_manifest, name, kind, pkg_name, &pkg_root)?;

    ctx.console().info(format!(
        "successfully removed {kind} dependency `{name}` from the project `{pkg_name}` at `{}`",
        pkg_root.display(),
    ))?;
    Ok(())
}

/// Remove a dependency or provide a meaningful error.
fn remove_dep(
    editable_manifest: &EditableManifest,
    name: &str,
    kind: DependencyKind,
    pkg_name: StrId,
    pkg_root: &Path,
) -> QuackResult<()> {
    match editable_manifest.remove_dependency(name, kind)? {
        DependencyRemoved::Yes => Ok(()),
        DependencyRemoved::NoDependency => qp_bail!(
            "no such {kind} dependency as `{name}` in the project `{pkg_name}` at `{}`",
            pkg_root.display()
        ),
        DependencyRemoved::NoDependencyButKindExists(other_kind) => {
            let err = if matches!(other_kind, DependencyKind::Normal) {
                Err(QuackError::hint(
                    "if you want to remove a normal dependency don't use any flags",
                ))
            } else {
                Err(QuackError::hint(format!(
                    "if you want to remove a {other_kind} dependency use flag `--{other_kind}`"
                )))
            };
            err.with_context(|| {
                format!(
                    "no such {kind} dependency as `{name}` in the project `{pkg_name}` at `{}`",
                    pkg_root.display()
                )
            })
        }
    }
}
