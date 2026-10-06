// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! The goal of module is to turn CLI-specified description of the dependency to add,
//! into a pair that can be directly inserted into the manifest schema.
//!
//! This is done in three steps.
//! 1. First, counterintuively, we construct a [`Source`] from the CLI-specification.
//! 2. Then we use that source and the rest of the specification to fetch the dependency, to check that it exists.
//!    Using [`Source`] allows us to use the already implemented [`Gatherer`] machinery instead of duplicating it.
//! 3. Lastly we downcast [`Source`] into [`SourceSchema`].
//!
//! Note that this is different from parsing, when the general workflow is `raw string -> schema -> full type`.
//!
//! Important
//! ---------
//! 1. If the dependency is a registry dependency and the user did not specify any versions,
//!    we interpret it as a request for the latest version / something compatible with it.
//! 2. If the user specifies a local dependency by a relative path,
//!    the path is relative to the folder in which `duck add` was called,
//!    not to the folder in which the project was found.
//!    This is taken into account, as we modify such path to be relative to the right folder.
use std::cell::RefCell;
use std::debug_assert;
use std::path::{Path, PathBuf};

use futures::executor::block_on;

use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::solver::gathering::fetch_types::{
    FetchFailure, FetchResponse, ManifestsRequest, NotPinnedRequest, PinnedRequest,
    RequestIdentifier,
};
use crate::quackpack::core::solver::gathering::gatherer::Gatherer;
use crate::quackpack::core::storage::git_access::StorageGitAccess;
use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::core::{GitReference, PackageContext, Source, SourceKind, Version};
use crate::quackpack::schemas::manifest::{
    Dependency, DependencyFeature, DependencySource as SourceSchema, DetailedSource, OredSemver,
};
use crate::quackpack::util::to_path_buf::ToPathBuf;
use crate::quackpack::util::to_url::ToUrl;
use crate::util::error::ErrorsLogger;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult, QuackResultContext, StrId, qp_bail};

#[derive(Debug, Clone)]
/// Raw specification from which [`Dependency`] can be constructed.
pub struct DependencySpecification<'matches> {
    /// Specification of the name of the dependency.
    pub name_spec: NameSpecification<'matches>,
    /// Specification of the source of the dependency.
    pub source_spec: SourceSpecification<'matches>,
    /// Required versions of this dependency.
    pub versions: Vec<Version>,
    /// Features of the dependency.
    pub features: Vec<&'matches str>,
    /// Whether the dependency should be pinned.
    pub pinned: bool,
}

#[derive(Debug, Clone)]
/// Raw specification from which [`DependencySource`] can be constructed.
pub struct SourceSpecification<'matches> {
    pub local_path: Option<&'matches Path>,
    pub git_url: Option<&'matches str>,
    pub git_branch: Option<&'matches str>,
    pub git_tag: Option<&'matches str>,
    pub git_commit: Option<&'matches str>,
    pub registry_url: Option<&'matches str>,
}

#[derive(Debug, Clone)]
/// Specification of the name of the dependency.
pub struct NameSpecification<'matches> {
    pub name: &'matches str,
    pub alias: Option<&'matches str>,
}

impl NameSpecification<'_> {
    /// How this dependency will be referenced in manifest and in code.
    fn effective_name(&self) -> String {
        self.alias.unwrap_or(self.name).to_string()
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
    pcx: &PackageContext,
) -> QuackResult<(String, Dependency)> {
    let DependencySpecification {
        name_spec,
        source_spec,
        mut versions,
        features,
        pinned,
    } = dep_spec;
    // Dependency's source.
    let (source, local_path) = construct_source(source_spec, pcx.package().root(), pcx.ctx())?;
    disallow_adding_itself(pcx.package().root(), source)?;

    // For now it is not possible to specify feature conditions through `add` interface.
    let features: Vec<DependencyFeature> = features
        .into_iter()
        .map(|f| DependencyFeature::Simple(f.to_string()))
        .collect();
    let latest_version = verify_and_get_version(
        name_spec.name.into(),
        source,
        &versions,
        &features,
        pinned,
        pcx,
    )?;
    if source.is_registry() && versions.is_empty() {
        // Adding registry dependency without versions is treated as adding the dependency in the latest version.
        versions = vec![latest_version];
    }
    let source = construct_source_schema(source, local_path, &name_spec)?;
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
    Ok((dep_key, dep))
}

/// Construct [`Source`] from the given [`SourceSpecification`].
/// Since [`Source`] has absolute paths but if the user provided a relative path,
/// we should insert a relative path to the manifest,
/// this function also returns the path that should be inserted given the dependency is a local one.
fn construct_source(
    spec: SourceSpecification,
    pkg_root: &Path,
    ctx: &DuckContext,
) -> QuackResult<(Source, Option<PathBuf>)> {
    let SourceSpecification {
        local_path,
        git_url,
        git_branch,
        git_tag,
        git_commit,
        registry_url,
    } = spec;
    if let Some(path) = local_path {
        // Local dependency.
        debug_assert!(
            git_url.is_none() && registry_url.is_none(),
            "guarded by the parser"
        );
        // We take into account that `local_path` is relative to `cwd`, not to `pkg_root`.
        let from_project_to_cwd = pkg_root.resolve_both_and_get_relative(ctx.cwd(), ctx);
        let from_project_to_dep = from_project_to_cwd.join(path).normalize();
        let dep_resolved = from_project_to_dep.resolve(ctx);
        Ok((Source::for_local(&dep_resolved)?, Some(from_project_to_dep)))
    } else if let Some(git_url) = git_url {
        // Git dependency.
        debug_assert!(registry_url.is_none(), "guarded by the parser");
        let git_url = git_url.to_url()?;
        let git_ref = construct_git_ref(git_tag, git_branch, git_commit);
        Ok((Source::for_git(git_url, git_ref), None))
    } else if let Some(registry_url) = registry_url {
        // Registry dependency.
        let registry_url = registry_url.to_url()?;
        Ok((Source::for_registry(registry_url), None))
    } else {
        // Registry dependency from default registry.
        let default_registry_url = Fetcher::DEFAULT_REGISTRY_URL
            .to_url()
            .context_internal("default registry url is invalid")?;
        Ok((Source::for_registry(default_registry_url), None))
    }
}

/// Construct a [`GitReference`] from the given specification.
fn construct_git_ref(
    tag: Option<&str>,
    branch: Option<&str>,
    commit: Option<&str>,
) -> GitReference {
    if let Some(tag) = tag {
        debug_assert!(
            branch.is_none() && commit.is_none(),
            "guarded by the parser"
        );
        GitReference::Tag(tag.into())
    } else if let Some(branch) = branch {
        debug_assert!(commit.is_none(), "guarded by the parser");
        GitReference::Branch(branch.into())
    } else if let Some(commit) = commit {
        GitReference::Rev(commit.into())
    } else {
        GitReference::Default
    }
}

/// Check that the dependency exists as specified by the user.
/// This is done by asking [`Gatherer`] to fetch the dependency.
/// Returns the newest found version satisfying the requirements.
fn verify_and_get_version(
    name: StrId,
    source: Source,
    versions: &[Version],
    features: &[DependencyFeature],
    pinned: bool,
    pcx: &PackageContext,
) -> QuackResult<Version> {
    let errors = RefCell::new(ErrorsLogger::default());
    let fetcher = Fetcher::new(pcx.ctx())?;
    let storage = Storage::new(pcx.storage_path());
    let access = StorageGitAccess::new(&storage);

    let gatherer = Gatherer::new(&fetcher, &access);
    let request = construct_request(name, source, versions, features, pinned)?;

    let fetch_response = block_on(gatherer.fetch(request, &errors))?;
    let errors = errors.take();
    if errors.has_errors() {
        qp_bail!(errors.unwrap_first())
    }

    match fetch_response {
        FetchResponse::Success(response) => Ok(response.latest_version()?),
        FetchResponse::Failed(failure) => {
            let id = match failure {
                FetchFailure::NotPinned(not_pinned) => not_pinned.origin_id,
                FetchFailure::Pinned(pinned) => pinned.origin_id,
            };
            qp_bail!(
                "failed to fetch dependency {} from `{}`",
                id.name,
                id.source.url()
            )
        }
    }
}

/// Construct the [`ManifestRequest`] used in [`verify_and_get_version`].
fn construct_request(
    name: StrId,
    source: Source,
    versions: &[Version],
    features: &[DependencyFeature],
    pinned: bool,
) -> QuackResult<ManifestsRequest> {
    let id = RequestIdentifier { name, source };
    let features = features
        .iter()
        .map(|f| match f {
            DependencyFeature::Simple(simple) => simple.into(),
            DependencyFeature::Detailed(detailed) => (&detailed.0.key).into(),
        })
        .collect();
    if pinned {
        if let Some(version) = versions.first()
            && versions.len() == 1
        {
            Ok(ManifestsRequest::Pinned(PinnedRequest {
                id,
                version: *version,
                features,
            }))
        } else {
            qp_bail!("pinned dependency should have exactly one version specified");
        }
    } else {
        let versions = if versions.is_empty() {
            None
        } else {
            Some(versions.to_vec())
        };
        Ok(ManifestsRequest::NotPinned(NotPinnedRequest {
            id,
            versions,
            features,
        }))
    }
}

/// Downcast [`Source`] to [`SourceSchema`].
fn construct_source_schema(
    source: Source,
    local_path: Option<PathBuf>,
    name_spec: &NameSpecification,
) -> QuackResult<Option<SourceSchema>> {
    let name = name_spec.source_name();
    match source.kind() {
        SourceKind::Git(git_ref) => {
            let (branch, tag, commit) = match git_ref {
                GitReference::Branch(b) => (Some(b.to_string()), None, None),
                GitReference::Tag(t) => (None, Some(t.to_string()), None),
                GitReference::Rev(r) => (None, None, Some(r.to_string())),
                GitReference::Default => (None, None, None),
            };
            Ok(Some(SourceSchema::Detailed(DetailedSource {
                registry_url: None,
                name,
                path: None,
                git_url: Some(source.url().to_string()),
                tag,
                branch,
                commit,
            })))
        }
        SourceKind::Registry => {
            let registry_url = source.url().to_string();
            if name.is_none() {
                if registry_url == Fetcher::DEFAULT_REGISTRY_URL {
                    Ok(None)
                } else {
                    Ok(Some(SourceSchema::Simple(registry_url)))
                }
            } else {
                Ok(Some(SourceSchema::Detailed(DetailedSource {
                    registry_url: Some(registry_url),
                    name,
                    path: None,
                    git_url: None,
                    tag: None,
                    commit: None,
                    branch: None,
                })))
            }
        }
        SourceKind::Local => {
            let path = local_path.with_context_internal(|| {
                format!("source {source:?} is local but no `relative_path` set")
            })?;
            Ok(Some(SourceSchema::Detailed(DetailedSource {
                registry_url: None,
                name,
                path: Some(path),
                git_url: None,
                tag: None,
                commit: None,
                branch: None,
            })))
        }
    }
}

fn disallow_adding_itself(root: &Path, dep_source: Source) -> QuackResult<()> {
    if let Ok(dep_path) = dep_source.url().to_path_buf()
        && root == dep_path
    {
        qp_bail!("tried to add root package as its own dependency")
    }
    Ok(())
}
