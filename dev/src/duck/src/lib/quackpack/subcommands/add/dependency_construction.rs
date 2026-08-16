use std::path::{Path, PathBuf};

use crate::DuckContext;
use crate::quackpack::core::Version;
use crate::quackpack::schemas::manifest::{
    Dependency, DependencyFeature, DependencySource, DetailedSource, OredSemver,
};
use crate::util::path_ops_ext::PathOpsExt;

#[derive(Debug, Clone)]
/// Raw specification from which [`Dependency`] can be constructed.
pub struct DependencySpecification {
    /// Specification of the name of the dependency.
    pub name_spec: NameSpecification,
    /// Specification of the source of the dependency.
    pub source_spec: SourceSpecification,
    /// Required versions of this dependency.
    pub versions: Vec<Version>,
    /// Features of the dependency.
    pub features: Vec<String>,
    /// Whether the dependency should be pinned.
    pub pinned: bool,
}

#[derive(Debug, Clone)]
/// Raw specification from which [`DependencySource`] can be constructed.
pub struct SourceSpecification {
    pub local_path: Option<PathBuf>,
    pub git_url: Option<String>,
    pub git_branch: Option<String>,
    pub git_tag: Option<String>,
    pub git_commit: Option<String>,
    pub registry_url: Option<String>,
}

#[derive(Debug, Clone)]
/// Specification of the name of the dependency.
pub struct NameSpecification {
    pub name: String,
    pub alias: Option<String>,
}

impl NameSpecification {
    /// How this dependency will be referenced in manifest and in code.
    fn effective_name(self) -> String {
        self.alias.unwrap_or(self.name)
    }

    /// Whether and how [`DependencySource`] should specify a name.
    fn source_name(&self) -> Option<String> {
        if self.alias.is_some() {
            Some(self.name.to_string())
        } else {
            None
        }
    }
}

/// Construct the appropriate [`Dependency`] object, specified by input.
/// Returns a pair of [`String`] and [`Dependency`], which can be treated as an entry in `dependencies` or `dev-dependencies` maps.
pub fn construct_dependency(
    dep_spec: DependencySpecification,
    pkg_root: &Path,
    ctx: &DuckContext,
) -> (String, Dependency) {
    let DependencySpecification {
        name_spec,
        source_spec,
        versions,
        features,
        pinned,
    } = dep_spec;
    // Dependency's source.
    let source = construct_source(&name_spec, source_spec, pkg_root, ctx);
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
    let dep_key = name_spec.effective_name();
    (dep_key, dep)
}

/// Construct [`DependencySource`] from the given [`SourceSpecification`].
fn construct_source(
    name_spec: &NameSpecification,
    spec: SourceSpecification,
    pkg_root: &Path,
    ctx: &DuckContext,
) -> Option<DependencySource> {
    // Whether and how the source should specify the name of the dependency.
    let source_name = name_spec.source_name();
    let SourceSpecification {
        local_path,
        git_url,
        git_branch,
        git_tag,
        git_commit,
        registry_url,
    } = spec;
    // We take into account that `local_path` is relative to `cwd`, not to `pkg_root`.
    let local_path = local_path.map(|path| {
        let from_project_to_cwd = pkg_root.resolve_both_and_get_relative(ctx.cwd(), ctx);
        from_project_to_cwd.join(path)
    });
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
