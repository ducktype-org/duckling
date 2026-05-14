use std::collections::{HashMap, HashSet};

use crate::quackpack::core::solver::types_common::{
    ExpandedPackage, InternedExpandedLocation, InternedLocation, Location, Package,
};
use crate::quackpack::core::version::CompatibilityCheck;
use crate::quackpack::core::{Dependency, Version};
use crate::{QuackResult, QuackResultContext};

/// For a given dpendency entry from the manifest and
/// given all the found versions of a package from some location,
/// find all the packages satisfying the dependency.
pub fn get_possible_realizations(
    dependency_description: &Dependency,
    versions_for_location: &HashMap<InternedExpandedLocation, HashSet<Option<Version>>>,
    location_resolver: &HashMap<InternedLocation, InternedExpandedLocation>,
) -> QuackResult<Vec<ExpandedPackage>> {
    if dependency_description.is_pinned() {
        // For a pinned dependency only one package can be a realization.
        let version = dependency_description
            .versions()
            .first()
            .context_internal("Pinned dependency should have exactly one version specified")?;
        let only_package = Package {
            location: InternedLocation::new(Location::from(dependency_description)),
            version: Some(*version),
        }
        .resolve(location_resolver);
        Ok(only_package.iter().cloned().collect())
    } else {
        let Some(location) = location_resolver.get(&InternedLocation::new(Location::from(
            dependency_description,
        ))) else {
            return Ok(vec![]);
        };
        // Baseline versions are the versions specified in the manifest,
        // with which we want to check the compatibility of the existing packages.
        let baseline_versions = if location.is_local() || location.is_git() {
            vec![None]
        } else {
            dependency_description
                .versions()
                .iter()
                .copied()
                .map(Some)
                .collect()
        };
        let Some(possible_versions) = versions_for_location.get(location) else {
            return Ok(vec![]);
        };
        let good_versions: Vec<Option<Version>> = possible_versions
            .iter()
            .filter(|version| {
                baseline_versions
                    .iter()
                    .any(|baseline| baseline.can_be_upgraded_to(*version))
            })
            .copied()
            .collect();
        Ok(good_versions
            .into_iter()
            .map(|version| ExpandedPackage {
                location: *location,
                version,
            })
            .collect())
    }
}

#[cfg(test)]
mod test {
    use std::collections::{HashMap, HashSet};
    use std::path::PathBuf;

    use tempfile::{TempDir, tempdir};
    use url::Url;

    use crate::quackpack::core::solver::types_common::{
        ExpandedLocation, ExpandedPackage, Location,
    };
    use crate::quackpack::core::solver::util::get_possible_realizations;
    use crate::quackpack::core::{Version, parse_manifest};
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
        let location_b = Location::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        }
        .into();
        let exp_location_b = ExpandedLocation::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        }
        .into();
        let location_resolver = HashMap::from([(location_b, exp_location_b)]);
        let versions_for_location = HashMap::from([(
            exp_location_b,
            HashSet::from([
                Some(Version::new(0, 0, 1)),
                Some(Version::new(1, 0, 0)),
                Some(Version::new(1, 0, 3)),
                Some(Version::new(1, 0, 5)),
                Some(Version::new(1, 3, 3)),
                Some(Version::new(2, 0, 3)),
            ]),
        )]);
        let res = get_possible_realizations(dependency, &versions_for_location, &location_resolver)
            .unwrap();
        assert_eq!(
            res,
            vec![ExpandedPackage {
                location: exp_location_b,
                version: Some(Version::new(1, 0, 3))
            }]
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
        let location_b = Location::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        }
        .into();
        let exp_location_b = ExpandedLocation::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        }
        .into();
        let location_resolver = HashMap::from([(location_b, exp_location_b)]);
        let versions_for_location = HashMap::from([(
            exp_location_b,
            HashSet::from([
                Some(Version::new(0, 0, 1)),
                Some(Version::new(1, 0, 0)),
                Some(Version::new(1, 0, 3)),
                Some(Version::new(1, 0, 5)),
                Some(Version::new(1, 3, 3)),
                Some(Version::new(2, 0, 3)),
            ]),
        )]);
        let res = get_possible_realizations(dependency, &versions_for_location, &location_resolver)
            .unwrap();
        assert_eq!(
            HashSet::from_iter(res),
            HashSet::from([
                ExpandedPackage {
                    location: exp_location_b,
                    version: Some(Version::new(1, 0, 3))
                },
                ExpandedPackage {
                    location: exp_location_b,
                    version: Some(Version::new(1, 0, 5))
                },
                ExpandedPackage {
                    location: exp_location_b,
                    version: Some(Version::new(1, 3, 3))
                }
            ])
        );
    }
}
