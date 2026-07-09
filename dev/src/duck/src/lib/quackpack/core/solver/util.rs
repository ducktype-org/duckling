use std::collections::{HashMap, HashSet};

use crate::quackpack::core::full_identity::{FullIdentity, FullKind, FullOrigin};
use crate::quackpack::core::solver::gathering::fetch_types::{
    ManifestsRequest, NotPinnedRequest, PinnedRequest, RequestIdentifier,
};
use crate::quackpack::core::version::CompatibilityCheck;
use crate::quackpack::core::{Dependency, GitReference, Source, SourceKind, Version};
use crate::quackpack::util::with_version::WithVersion;
use crate::{QuackResult, QuackResultContext};

/// For a given dependency entry from the manifest and
/// given all the found versions of a package with a given identity,
/// find all the packages satisfying the dependency.
pub fn get_possible_realizations(
    dependency_description: &Dependency,
    versions_for_identity: &HashMap<FullIdentity, HashSet<Version>>,
    source_to_origin_resolver: &HashMap<Source, FullOrigin>,
) -> QuackResult<Vec<WithVersion<FullIdentity>>> {
    let Some(origin) = source_to_origin_resolver.get(dependency_description.source()) else {
        return Ok(vec![]);
    };
    let identity = FullIdentity::new(dependency_description.name(), *origin);
    if dependency_description.is_pinned() {
        // For a pinned dependency only one package can be a realization.
        let versions = dependency_description
            .versions();
        if versions.len() != 1 {
            return Ok(vec![]);
        }
        let version = versions[0];
        Ok(vec![WithVersion::new(identity, version)])
    } else {
        // Baseline versions are the versions specified in the manifest,
        // with which we want to check the compatibility of the existing packages.
        // If they are empty, then there are no constraints on the version, so all versions are ok.
        let baseline_versions = dependency_description.versions();
        let Some(possible_versions) = versions_for_identity.get(&identity) else {
            return Ok(vec![]);
        };
        let good_versions: Vec<Version> = if baseline_versions.is_empty() {
            possible_versions.iter().copied().collect()
        } else {
            possible_versions
                .iter()
                .filter(|version| {
                    baseline_versions
                        .iter()
                        .any(|baseline| baseline.can_be_upgraded_to(*version))
                })
                .copied()
                .collect()
        };
        Ok(good_versions
            .into_iter()
            .map(|version| WithVersion::new(identity, version))
            .collect())
    }
}

impl Source {
    /// Creates a [`Source`] which could correspond to the given [`FullOrigin`].
    /// Used for generating mapping Source -> FullIdentity for packages from the previous freeze.
    pub fn canonical_source_for_origin(origin: FullOrigin) -> Self {
        match origin.kind() {
            FullKind::Registry => Self::for_registry(origin.url()),
            FullKind::Git { commit } => Self::for_git(origin.url(), GitReference::Rev(commit)),
            FullKind::Local => Self::new(origin.url(), SourceKind::Local),
        }
    }
}

impl WithVersion<FullIdentity> {
    /// Creates a [`ManifestsRequest`] for a package.
    /// Used for requesting fetches of packages from the previous freeze.
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

    /// Checks whether a dependency is still satisfied by the package.
    /// This consists of checking that:
    ///  * the packages origin satisfies the source requirements,
    ///  * version requirements are satisfied,
    ///  * package's name coincides with the required name.
    pub fn still_satisfies_dep(&self, dependency: &Dependency) -> QuackResult<bool> {
        if !self.check_satisfaction_of_versions(dependency)? {
            return Ok(false);
        }
        let source = dependency.source();
        if self.value().origin().url() != source.url() || self.value().name() != dependency.name() {
            return Ok(false);
        }
        match (self.value().origin().kind(), source.kind()) {
            (FullKind::Local, SourceKind::Local) => Ok(true),
            (FullKind::Git { commit }, SourceKind::Git(reference)) => {
                // If the git dependency specifies tag, branch or nothing (default branch),
                // some new commits may have appeared.
                if let GitReference::Rev(required_commit) = reference
                    && commit == *required_commit
                {
                    Ok(true)
                } else {
                    Ok(false)
                }
            }
            (FullKind::Registry, SourceKind::Registry) => Ok(true),
            _ => Ok(false),
        }
    }

    /// Helper for [`Self::still_satisfies_dep`].
    fn check_satisfaction_of_versions(&self, dependency: &Dependency) -> QuackResult<bool> {
        if dependency.is_pinned() {
            let required_version = dependency
                .versions()
                .first()
                .context_internal("Pinned dependency without specified version")?;
            Ok(self.version() == *required_version)
        } else {
            Ok(dependency
                .versions()
                .iter()
                .any(|required| required.can_be_upgraded_to(&self.version())))
        }
    }
}

impl WithVersion<RequestIdentifier> {
    pub fn resolve(
        self,
        source_to_origin_resolver: &HashMap<Source, FullOrigin>,
    ) -> Option<WithVersion<FullIdentity>> {
        source_to_origin_resolver
            .get(&self.value().source)
            .map(|origin| {
                WithVersion::new(
                    FullIdentity::new(self.value().name, *origin),
                    self.version(),
                )
            })
    }
}

#[cfg(test)]
mod test {
    use std::collections::{HashMap, HashSet};
    use std::path::PathBuf;

    use tempfile::{TempDir, tempdir};

    use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
    use crate::quackpack::core::solver::util::get_possible_realizations;
    use crate::quackpack::core::{Source, Version, parse_manifest};
    use crate::quackpack::util::to_url::ToUrl;
    use crate::quackpack::util::with_version::WithVersion;
    use crate::util::path_ops_ext::PathOpsExt;
    use crate::{DuckContext, StrId};

    fn prepare_manifest(contents: &str) -> (TempDir, PathBuf) {
        let dir = tempdir().unwrap();
        let manifest = dir.path().join("x");
        manifest.touch().unwrap();
        manifest.write(contents).unwrap();
        dir.path().try_fsync_dir().unwrap();
        (dir, manifest)
    }

    #[test]
    fn pinned() {
        let (_dir, manifest_path) = prepare_manifest(
            r#"
metadata:
  name: a
  version: '1'

dependencies:
  b:
    version: 1.0.3
    pinned: true
"#,
        );
        let ctx = DuckContext::default();
        let pkg = parse_manifest(&manifest_path, &ctx).unwrap();
        let manifest = pkg.manifest();
        let dependency = manifest
            .dependencies()
            .get_by_name(StrId::new("b"))
            .unwrap();
        let source_b = Source::for_registry("http://localhost:9001".to_url().unwrap());
        let origin_b = FullOrigin::for_registry("http://localhost:9001".to_url().unwrap());
        let source_to_origin_resolver = HashMap::from([(source_b, origin_b)]);
        let identity_b = FullIdentity::new("b".into(), origin_b);
        let versions_for_identity = HashMap::from([(
            identity_b,
            HashSet::from([
                Version::new(0, 0, 1),
                Version::new(1, 0, 0),
                Version::new(1, 0, 3),
                Version::new(1, 0, 5),
                Version::new(1, 3, 3),
                Version::new(2, 0, 3),
            ]),
        )]);
        let res = get_possible_realizations(
            dependency,
            &versions_for_identity,
            &source_to_origin_resolver,
        )
        .unwrap();
        assert_eq!(
            res,
            vec![WithVersion::new(identity_b, Version::new(1, 0, 3))]
        );
    }

    #[test]
    fn not_pinned() {
        let (_dir, manifest_path) = prepare_manifest(
            r#"
metadata:
  name: a
  version: '1'

dependencies:
  b:
    version: 1.0.3
"#,
        );
        let ctx = DuckContext::default();
        let pkg = parse_manifest(&manifest_path, &ctx).unwrap();
        let manifest = pkg.manifest();
        let dependency = manifest
            .dependencies()
            .get_by_name(StrId::new("b"))
            .unwrap();
        let source_b = Source::for_registry("http://localhost:9001".to_url().unwrap());
        let origin_b = FullOrigin::for_registry("http://localhost:9001".to_url().unwrap());
        let source_to_origin_resolver = HashMap::from([(source_b, origin_b)]);
        let identity_b = FullIdentity::new("b".into(), origin_b);
        let versions_for_identity = HashMap::from([(
            identity_b,
            HashSet::from([
                Version::new(0, 0, 1),
                Version::new(1, 0, 0),
                Version::new(1, 0, 3),
                Version::new(1, 0, 5),
                Version::new(1, 3, 3),
                Version::new(2, 0, 3),
            ]),
        )]);
        let res = get_possible_realizations(
            dependency,
            &versions_for_identity,
            &source_to_origin_resolver,
        )
        .unwrap();
        assert_eq!(
            HashSet::from_iter(res),
            HashSet::from([
                WithVersion::new(identity_b, Version::new(1, 0, 3)),
                WithVersion::new(identity_b, Version::new(1, 0, 5)),
                WithVersion::new(identity_b, Version::new(1, 3, 3)),
            ])
        );
    }
}
