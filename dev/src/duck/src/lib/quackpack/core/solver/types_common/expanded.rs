use std::hash::Hash;

use serde::{Deserialize, Serialize};
use url::Url;

use crate::quackpack::core::full_identity::{FullIdentity, FullKind};
use crate::quackpack::core::solver::gathering::fetch_types::{
    ManifestsRequest, NotPinnedRequest, PinnedRequest, RequestIdentifier,
};
use crate::quackpack::core::version::CompatibilityCheck;
use crate::quackpack::core::{Dependency, GitReference, Source, SourceKind, Version};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::to_path_buf::ToPathBuf;
use crate::quackpack::util::with_version::WithVersion;
use crate::{QuackResult, QuackResultContext, StrId, qp_bail_internal};

#[derive(Debug, Clone, Copy, Deserialize, Eq, Hash, PartialEq, Serialize)]
/// Type describing a localization of a dependency.
/// Can either be:
/// * registry - a dependency with a given name from a given server;
/// * git - a dependency on a commit of a git repository at a given URL;
/// * local - a dependency on a package that is stored locally on disk.
///
/// Difference between [`Location`] and [`ExpandedLocation`]
/// -------------------------------------------------------
/// [`Location`] directly corresponds to an entry in manifest.
/// Specifically, git dependencies can be specified by also tags or branches.
/// Thus different locations can actually specify the same package,
/// but it can only be known after cloning git repositories.
/// Thus firstly we use [`Location`] and after all the manifests are gathered,
/// we transition to using [`ExpandedLocation`].
pub enum ExpandedLocation {
    Registry { url: InternedUrl, real_name: StrId },
    Git { url: InternedUrl, commit: StrId },
    Local { absolute_path: InternedUrl },
}

impl ExpandedLocation {
    pub fn is_registry(&self) -> bool {
        matches!(self, Self::Registry { .. })
    }

    pub fn is_git(&self) -> bool {
        matches!(self, Self::Git { .. })
    }

    pub fn is_local(&self) -> bool {
        matches!(self, Self::Local { .. })
    }

    /// Return a descriptive name of this location.
    pub fn descriptive_name(&self) -> String {
        match self {
            Self::Registry { real_name, .. } => format!("`{}", real_name),
            Self::Git { url, .. } => format!("cloned from `{url}`"),
            Self::Local { absolute_path } => {
                if let Ok(path) = absolute_path.to_path_buf() {
                    format!("at the directory `{}`", path.display())
                } else {
                    format!("at the directory `{}`", absolute_path)
                }
            }
        }
    }
}

#[derive(Copy, Clone, Debug, Eq, Hash, PartialEq)]
/// Type describing a concrete package from the point of view of the solver.
/// This contains an [`ExpandedLocation`] and a version.
/// For git and local dependencies the version field is [`None`] and for registry
/// dependencies the version field contains the version of the dependency.
pub struct ExpandedPackage {
    pub location: ExpandedLocation,
    pub version: Option<Version>,
}

impl ExpandedPackage {
    pub fn location(&self) -> &ExpandedLocation {
        &self.location
    }

    pub fn version(&self) -> Option<Version> {
        self.version
    }

    pub fn is_compatible_with(&self, other: &ExpandedPackage) -> QuackResult<bool> {
        if !(self.location == other.location) {
            return Ok(false);
        }
        if self.location.is_registry() {
            let Some(version_self) = self.version else {
                qp_bail_internal!("Registry ExpandedPackage should have a version specified")
            };
            let Some(version_other) = other.version else {
                qp_bail_internal!("Registry ExpandedPackage should have a version specified")
            };
            Ok(version_other.can_be_upgraded_to(&version_self))
        } else {
            Ok(self.version == other.version)
        }
    }

    /// Assuming that [`self`] was a realization of some dependency, checks whether we can be certain it is still true.
    pub fn still_satisfies_dep(&self, dependency: &Dependency) -> QuackResult<bool> {
        let source = dependency.source();
        let dep_url = source.url();
        match (self.location, source.kind()) {
            (ExpandedLocation::Local { absolute_path }, SourceKind::Local) => {
                Ok(absolute_path == dep_url)
            }
            (ExpandedLocation::Git { url, commit }, SourceKind::Git(reference)) => {
                // If the git dependency specifies tag, branch or nothing (default branch),
                // some new commits may have appeared.
                if let GitReference::Rev(required_commit) = reference
                    && commit == *required_commit
                    && url == dep_url
                {
                    if let Some(required_version) = dependency.versions().first() {
                        Ok(self.version == Some(*required_version))
                    } else {
                        Ok(true)
                    }
                } else {
                    Ok(false)
                }
            }
            (ExpandedLocation::Registry { url, real_name }, SourceKind::Registry) => {
                self.check_satisfaction_for_registry(&url, real_name, dep_url, dependency)
            }
            _ => Ok(false),
        }
    }

    /// Helper for [`Self::still_satisfies_dep`].
    fn check_satisfaction_for_registry(
        &self,
        url: &Url,
        real_name: StrId,
        dep_url: InternedUrl,
        dependency: &Dependency,
    ) -> QuackResult<bool> {
        let location_agreement = (url == dep_url) && (dependency.name() == real_name);
        let self_version = self
            .version
            .context_internal("Registry package with no version")?;
        if dependency.is_pinned() {
            let required_version = dependency
                .versions()
                .first()
                .context_internal("Pinned dependency without specified version")?;
            Ok(location_agreement && self_version == *required_version)
        } else {
            Ok(location_agreement
                && dependency
                    .versions()
                    .iter()
                    .any(|required| required.can_be_upgraded_to(&self_version)))
        }
    }
}

impl WithVersion<FullIdentity> {
    pub fn create_manifest_request(&self) -> QuackResult<ManifestsRequest> {
        let identity = self.value();
        let origin = identity.origin();
        let request_id = RequestIdentifier {
            source: Source::canonical_source_for_origin(origin),
            name: identity.name(),
        };
        match origin.kind() {
            FullKind::Registry => Ok(ManifestsRequest::Pinned(PinnedRequest {
                id: request_id,
                version: self.version(),
                features: [].into(),
            })),
            FullKind::Git { .. } => Ok(ManifestsRequest::NotPinned(NotPinnedRequest {
                id: request_id,
                versions: Some(vec![self.version()]),
                features: [].into(),
            })),
            FullKind::Local => Ok(ManifestsRequest::NotPinned(NotPinnedRequest {
                id: request_id,
                versions: Some(vec![self.version()]),
                features: [].into(),
            })),
        }
    }

    pub fn still_satisfies_dep(&self, dependency: &Dependency) -> QuackResult<bool> {
        let source = dependency.source();
        let dep_url = source.url();
        match (self.value().origin().kind(), source.kind()) {
            (FullKind::Local, SourceKind::Local) => Ok(self.value().origin().url() == dep_url),
            (FullKind::Git { commit }, SourceKind::Git(reference)) => {
                // If the git dependency specifies tag, branch or nothing (default branch),
                // some new commits may have appeared.
                if let GitReference::Rev(required_commit) = reference
                    && commit == *required_commit
                    && self.value().origin().url() == dep_url
                {
                    if let Some(required_version) = dependency.versions().first() {
                        Ok(self.version() == *required_version)
                    } else {
                        Ok(true)
                    }
                } else {
                    Ok(false)
                }
            }
            (FullKind::Registry, SourceKind::Registry) => self.check_satisfaction_for_registry(
                self.value().origin().url(),
                self.value().name(),
                dep_url,
                dependency,
            ),
            _ => Ok(false),
        }
    }

    /// Helper for [`Self::still_satisfies_dep`].
    fn check_satisfaction_for_registry(
        &self,
        url: InternedUrl,
        real_name: StrId,
        dep_url: InternedUrl,
        dependency: &Dependency,
    ) -> QuackResult<bool> {
        let location_agreement = (url == dep_url) && (dependency.name() == real_name);
        if dependency.is_pinned() {
            let required_version = dependency
                .versions()
                .first()
                .context_internal("Pinned dependency without specified version")?;
            Ok(location_agreement && self.version() == *required_version)
        } else {
            Ok(location_agreement
                && dependency
                    .versions()
                    .iter()
                    .any(|required| required.can_be_upgraded_to(&self.version())))
        }
    }
}
