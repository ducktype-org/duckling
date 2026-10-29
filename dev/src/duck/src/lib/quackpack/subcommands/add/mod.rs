// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::path::Path;

use crate::quackpack::core::editable_manifest::{DependencyAdded, EditableManifest};
use crate::quackpack::core::{AllowGlobalPackage, DependencyKind, PackageLoader};
use crate::quackpack::schemas::manifest::Dependency;
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext, StrId, qp_bail};

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
    let has_alias = dep_spec.name_spec.alias.is_none();
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
        has_alias,
        pkg_name,
        &pkg_root,
    )?;
    editable_manifest.save().with_context(|| format!("when trying to add the {kind} dependency `{effective_name}` to the project `{pkg_name}` at `{}`", pkg_root.display()))?;
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
    has_alias: bool,
    pkg_name: StrId,
    pkg_root: &Path,
) -> QuackResult<()> {
    match editable_manifest.add_dependency(name, dep, kind)? {
        DependencyAdded::Yes => Ok(()),
        DependencyAdded::AlreadyExists(other_kind) => {
            // If no alias, add hint to use them.
            if has_alias {
                let err: Result<(), QuackError> = Err(QuackError::hint(
                    "use aliases to have multiple dependencies with the same name",
                ));
                err.with_context(|| {
                    format!(
                        "there already is a {other_kind} dependency referred to by `{name}` in the project `{pkg_name}` at `{}`",
                        pkg_root.display()
                    )
                })
            } else {
                qp_bail!(
                    "there already is a {other_kind} dependency referred to by `{name}` in the project `{pkg_name}` at `{}`",
                    pkg_root.display()
                )
            }
        }
    }
}
