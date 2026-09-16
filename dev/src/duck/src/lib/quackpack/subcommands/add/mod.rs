use std::path::Path;

use crate::quackpack::core::editable_manifest::{DependencyAdded, EditableManifest};
use crate::quackpack::core::{AllowGlobalPackage, DependencyKind, PackageLoader};
use crate::quackpack::schemas::manifest::Dependency;
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext, StrId};

mod dependency_construction;
use dependency_construction::construct_dependency;
pub use dependency_construction::{
    DependencySpecification, NameSpecification, SourceSpecification,
};

#[derive(Debug, Clone)]
/// All options that can be passed to `add`.
pub struct AddOptions<'matches> {
    /// Specification of the dependency.
    pub dep_spec: DependencySpecification<'matches>,
    /// Use a global package instead of a local one.
    pub global: bool,
    /// Add a dev-dependency.
    pub kind: DependencyKind,
}

/// Logic for executing the `add` subcommand.
pub fn add(ctx: &DuckContext, options: AddOptions) -> QuackResult<()> {
    let AddOptions {
        dep_spec,
        global,
        kind,
    } = options;
    let pcx = if global {
        PackageLoader::global_package(ctx)?
    } else {
        PackageLoader::find_from_cwd(ctx, AllowGlobalPackage::No)?
    };
    let (effective_name, dep) = construct_dependency(dep_spec, &pcx)?;

    let pkg = pcx.package().get_package();

    // Only necessary for diagnostic messages.
    let pkg_name = pkg.name();
    let pkg_root = pkg.root_directory().to_path_buf();

    let editable_manifest = EditableManifest::new(&pcx)?;
    add_dep(
        &editable_manifest,
        &effective_name,
        dep,
        kind,
        pkg_name,
        &pkg_root,
    )?;
    editable_manifest.save()?;
    ctx.info(
        format!(
        "successfully added {kind} dependency `{effective_name}` to the project `{pkg_name}` at `{}`",
        pkg_root.display(),
    ))?;
    Ok(())
}

/// Add a dependency or provide a meaningful error.
fn add_dep(
    editable_manifest: &EditableManifest,
    name: &str,
    dep: Dependency,
    kind: DependencyKind,
    pkg_name: StrId,
    pkg_root: &Path,
) -> QuackResult<()> {
    match editable_manifest.add_dependency(name, dep, kind)? {
        DependencyAdded::Yes => Ok(()),
        DependencyAdded::AlreadyExists => {
            let err: Result<(), QuackError> = Err(QuackError::hint(
                "use aliases to have multiple dependencies with the same name",
            ));
            err.with_context(|| {
                format!(
                    "{kind} dependency `{name}` already exists in the project `{pkg_name}` at `{}`",
                    pkg_root.display()
                )
            })
        }
    }
}
