use std::collections::{HashMap, HashSet};

use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
use crate::quackpack::core::version::CompatibilityCheck;
use crate::quackpack::core::{Dependency, Source, Version};
use crate::quackpack::util::with_version::WithVersion;
use crate::{QuackResult, QuackResultContext, StrId};

impl WithVersion<FullOrigin> {
    pub fn transpose_to_identity(&self, name: StrId) -> WithVersion<FullIdentity> {
        WithVersion::new(FullIdentity::new(name, *self.value()), self.version())
    }
}

/// For a given dependency entry from the manifest and
/// given all the found versions of a package from some location,
/// find all the packages satisfying the dependency.
pub fn get_possible_realizations(
    dependency_description: &Dependency,
    versions_for_location: &HashMap<FullIdentity, HashSet<Version>>,
    location_resolver: &HashMap<Source, FullOrigin>,
) -> QuackResult<Vec<WithVersion<FullIdentity>>> {
    let Some(origin) = location_resolver.get(dependency_description.source()) else {
        return Ok(vec![]);
    };
    let identity = FullIdentity::new(dependency_description.name(), *origin);
    if dependency_description.is_pinned() {
        // For a pinned dependency only one package can be a realization.
        let version = dependency_description
            .versions()
            .first()
            .context_internal("Pinned dependency should have exactly one version specified")?;
        Ok(vec![WithVersion::new(identity, *version)])
    } else {
        // Baseline versions are the versions specified in the manifest,
        // with which we want to check the compatibility of the existing packages.
        // If they are empty, then there are no constraints on the version, so all versions are ok.
        let baseline_versions = dependency_description.versions();
        let Some(possible_versions) = versions_for_location.get(&identity) else {
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
        let location_resolver = HashMap::from([(source_b, origin_b)]);
        let identity_b = FullIdentity::new("b".into(), origin_b);
        let versions_for_location = HashMap::from([(
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
        let res = get_possible_realizations(dependency, &versions_for_location, &location_resolver)
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
        let location_resolver = HashMap::from([(source_b, origin_b)]);
        let identity_b = FullIdentity::new("b".into(), origin_b);
        let versions_for_location = HashMap::from([(
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
        let res = get_possible_realizations(dependency, &versions_for_location, &location_resolver)
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
