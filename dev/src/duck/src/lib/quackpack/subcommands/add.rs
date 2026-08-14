use std::debug_assert;
use std::path::{Path, PathBuf};

use crate::quackpack::core::{AllowGlobalPackage, PackageLoader, Version};
use crate::quackpack::schemas::manifest::{
    Dependency, DependencyAdded, DependencyFeature, DependencySource, DetailedSource,
    Manifest as ManifestSchema, OredSemver,
};
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext, StrId};

#[derive(Debug, Clone)]
/// All options that can be passed to `add`.
pub struct AddOptions {
    /// Name of the dependency to add.
    pub name: String,
    /// How the dependency should be aliased.
    pub alias: Option<String>,
    /// Use a global package instead of a local one.
    pub global: bool,
    /// Add a dev-dependency.
    pub dev_dep: bool,
    /// Source of the dependency.
    pub source_spec: SourceSpecification,
    /// Required versions of this dependency.
    pub versions: Vec<Version>,
    /// Features of the dependency.
    pub features: Vec<String>,
    /// Whether the dependency should be pinned.
    pub pinned: bool,
}

#[derive(Debug, Clone)]
/// Raw specification of a [`DependencySource`].
pub struct SourceSpecification {
    pub local_path: Option<PathBuf>,
    pub git_url: Option<String>,
    pub git_branch: Option<String>,
    pub git_tag: Option<String>,
    pub git_commit: Option<String>,
    pub registry_url: Option<String>,
}

/// Logic for executing the `add` subcommand.
pub fn add(ctx: &DuckContext, options: AddOptions) -> QuackResult<()> {
    let AddOptions {
        name,
        alias,
        global,
        dev_dep,
        source_spec,
        versions,
        features,
        pinned,
    } = options;
    let (effective_name, dep) =
        build_dependency(name, alias, versions, source_spec, features, pinned);
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
        add_dev_dep(
            &mut schema,
            effective_name.clone(),
            dep,
            pkg_name,
            &pkg_root,
        )?;
    } else {
        add_normal_dep(
            &mut schema,
            effective_name.clone(),
            dep,
            pkg_name,
            &pkg_root,
        )?;
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
        "successfully added {}dependency `{effective_name}` to the project `{pkg_name}` at `{}`",
        if dev_dep { "dev-" } else { "" },
        pkg_root.display(),
    ))?;
    Ok(())
}

/// Construct the appropriate [`Dependency`] object, specified by input.
/// Returns a pair of [`String`] and [`Dependency`], which can be treated as an entry in `dependencies` or `dev-dependencies` maps.
fn build_dependency(
    name: String,
    alias: Option<String>,
    versions: Vec<Version>,
    source_spec: SourceSpecification,
    features: Vec<String>,
    pinned: bool,
) -> (String, Dependency) {
    // Dependency's source.
    let source = source_from_specification(&name, alias.as_ref(), source_spec);
    // Key in the `dependencies` or `dev-dependencies` map.
    let effective_name = alias.unwrap_or(name);
    // For now it is not possible to specify feature conditions through `add` interface.
    let features: Vec<DependencyFeature> = features
        .into_iter()
        .map(DependencyFeature::Simple)
        .collect();
    let dep = Dependency {
        version: if versions.is_empty() {
            None
        } else {
            Some(OredSemver(versions))
        },
        source,
        features: if features.is_empty() {
            None
        } else {
            Some(features)
        },
        pinned: if pinned { Some(true) } else { None },
        // For now it is not possible to specify dependency conditions through `add` interface.
        conditions: None,
    };
    (effective_name, dep)
}

/// Construct [`DependencySource`] from the given [`SourceSpecification`].
fn source_from_specification(
    name: &str,
    alias: Option<&String>,
    spec: SourceSpecification,
) -> Option<DependencySource> {
    // Whether and how the source should specify the name of the dependency.
    let source_name = if alias.is_some() {
        Some(name.to_string())
    } else {
        None
    };
    let SourceSpecification {
        local_path,
        git_url,
        git_branch,
        git_tag,
        git_commit,
        registry_url,
    } = spec;
    if source_name.is_none() && registry_url.is_none() && local_path.is_none() && git_url.is_none()
    {
        return None;
    }
    if source_name.is_none()
        && let Some(registry_url) = registry_url
    {
        debug_assert!(
            git_url.is_none() && local_path.is_none(),
            "guarded by the parser"
        );
        Some(DependencySource::Simple(registry_url))
    } else {
        Some(DependencySource::Detailed(DetailedSource {
            registry_url,
            name: source_name,
            path: local_path,
            git_url,
            tag: git_tag,
            commit: git_commit,
            branch: git_branch,
        }))
    }
}

/// Add a dev-dependency or provide a meaningful error.
fn add_dev_dep(
    schema: &mut ManifestSchema,
    name: String,
    dep: Dependency,
    pkg_name: StrId,
    pkg_root: &Path,
) -> QuackResult<()> {
    match schema.add_dev_dependency(name.clone(), dep) {
        DependencyAdded::Yes => Ok(()),
        DependencyAdded::AlreadyExists => {
            let err = Err(QuackError::hint(
                "use aliases to have multiple dev-dependencies with the same name",
            ));
            err.with_context(|| {
                format!(
                    "dev-dependency `{name}` already exists in the project `{pkg_name}` at `{}`",
                    pkg_root.display()
                )
            })
        }
    }
}

/// Add a dependency or provide a meaningful error.
fn add_normal_dep(
    schema: &mut ManifestSchema,
    name: String,
    dep: Dependency,
    pkg_name: StrId,
    pkg_root: &Path,
) -> QuackResult<()> {
    match schema.add_dependency(name.clone(), dep) {
        DependencyAdded::Yes => Ok(()),
        DependencyAdded::AlreadyExists => {
            let err = Err(QuackError::hint(
                "use aliases to have multiple dependencies with the same name",
            ));
            err.with_context(|| {
                format!(
                    "dependency `{name}` already exists in the project `{pkg_name}` at `{}`",
                    pkg_root.display()
                )
            })
        }
    }
}
