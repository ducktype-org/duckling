use std::path::Path;

use crate::quackpack::core::{AllowGlobalPackage, DependencyKind, PackageLoader};
use crate::quackpack::schemas::manifest::{
    Dependency, DependencyAdded, Manifest as ManifestSchema,
};
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext, StrId};

mod dependency_construction;
use dependency_construction::construct_dependency;
pub use dependency_construction::{
    DependencySpecification, NameSpecification, SourceSpecification,
};

#[derive(Debug, Clone)]
/// All options that can be passed to `add`.
pub struct AddOptions {
    /// Specification of the dependency.
    pub dep_spec: DependencySpecification,
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

    let pkg = pcx.into_package().unwrap_package();

    // Only necessary for diagnostic messages.
    let pkg_name = pkg.name();
    let pkg_root = pkg.root_directory().to_path_buf();

    // @TODO: #1394 We would like to use a better mechanism than modify deserialized schema -> blindly serialize it,
    // since this won't preserve comments and formatting choices in the manifest.
    let manifest_path = pkg.manifest_path().to_path_buf();
    let mut schema = pkg.into_original_schema();
    add_dep(
        &mut schema,
        effective_name.clone(),
        dep,
        kind,
        pkg_name,
        &pkg_root,
    )?;
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
        "successfully added {kind} dependency `{effective_name}` to the project `{pkg_name}` at `{}`",
        pkg_root.display(),
    ))?;
    Ok(())
}

/// Add a dependency or provide a meaningful error.
fn add_dep(
    schema: &mut ManifestSchema,
    name: String,
    dep: Dependency,
    kind: DependencyKind,
    pkg_name: StrId,
    pkg_root: &Path,
) -> QuackResult<()> {
    match schema.add_dependency(name.clone(), dep, kind) {
        DependencyAdded::Yes => Ok(()),
        DependencyAdded::AlreadyExists => {
            let err = Err(QuackError::hint(
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
