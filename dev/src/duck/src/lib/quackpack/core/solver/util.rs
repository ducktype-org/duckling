use std::collections::HashMap;

use crate::{
    QuackResult, QuackResultContext,
    quackpack::core::{
        Dependency, Version,
        types_common::{ExpandedLocation, ExpandedPackage, Location, Package},
        version::CompatibilityCheck,
    },
};

pub fn get_possible_realisations(
    dependency_description: &Dependency,
    versions_for_location: &HashMap<ExpandedLocation, Vec<Option<Version>>>,
    location_resolver: &HashMap<Location, ExpandedLocation>,
) -> QuackResult<Vec<ExpandedPackage>> {
    if dependency_description.is_pinned() {
        let version = dependency_description
            .desc()
            .versions()
            .first()
            .context_internal("Pinned dependency should have exactly one version specified")?;
        let only_package = Package {
            location: Location::from(dependency_description),
            version: Some(*version),
        }
        .resolve(location_resolver);
        Ok(only_package.iter().cloned().collect())
    } else {
        let Some(location) = location_resolver.get(&Location::from(dependency_description)) else {
            return Ok(vec![]);
        };
        let baseline_versions = if location.is_local() {
            vec![None]
        } else {
            dependency_description
                .desc()
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
                location: location.clone(),
                version,
            })
            .collect())
    }
}

#[cfg(test)]
mod test {
    use std::{collections::HashMap, path::PathBuf};

    use rustvil::fs::PathExt;
    use tempfile::{TempDir, tempdir};
    use url::Url;

    use crate::{
        DuckCtx, QpCtx, StrId,
        quackpack::core::{
            Version, parse_manifest,
            types_common::{
                ExpandedLocRegistry, ExpandedLocation, ExpandedPackage, LocRegistry, Location,
            },
            util::get_possible_realisations,
        },
    };

    fn prepare_manifest(contents: &str) -> (TempDir, PathBuf) {
        let dir = tempdir().unwrap();
        let manifest = dir.path().join("x");
        manifest.touch().unwrap();
        manifest.write(contents).unwrap();
        (dir, manifest)
    }

    #[test]
    fn pinned() {
        let (_dir, manifest_path) = prepare_manifest(
            r#"
metadata:
  name: a
  version: 1

dependencies:
  b:
    version: 1.0.3
    pinned: true
"#,
        );
        let ctx = DuckCtx::default();
        let pkg = parse_manifest(&manifest_path, &QpCtx::new(&ctx)).unwrap();
        let manifest = pkg.manifest();
        let dependency = manifest
            .dependencies()
            .all_dependencies()
            .get(&StrId::new("b"))
            .unwrap();
        let location_b = Location::Registry(LocRegistry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        });
        let exp_location_b = ExpandedLocation::Registry(ExpandedLocRegistry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        });
        let location_resolver = HashMap::from([(location_b.clone(), exp_location_b.clone())]);
        let versions_for_location = HashMap::from([(
            exp_location_b.clone(),
            vec![
                Some(Version::new(0, 0, 1)),
                Some(Version::new(1, 0, 0)),
                Some(Version::new(1, 0, 3)),
                Some(Version::new(1, 0, 5)),
                Some(Version::new(1, 3, 3)),
                Some(Version::new(2, 0, 3)),
            ],
        )]);
        let res =
            get_possible_realisations(&dependency, &versions_for_location, &location_resolver)
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
  version: 1

dependencies:
  b:
    version: 1.0.3
"#,
        );
        let ctx = DuckCtx::default();
        let pkg = parse_manifest(&manifest_path, &QpCtx::new(&ctx)).unwrap();
        let manifest = pkg.manifest();
        let dependency = manifest
            .dependencies()
            .all_dependencies()
            .get(&StrId::new("b"))
            .unwrap();
        let location_b = Location::Registry(LocRegistry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        });
        let exp_location_b = ExpandedLocation::Registry(ExpandedLocRegistry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        });
        let location_resolver = HashMap::from([(location_b.clone(), exp_location_b.clone())]);
        let versions_for_location = HashMap::from([(
            exp_location_b.clone(),
            vec![
                Some(Version::new(0, 0, 1)),
                Some(Version::new(1, 0, 0)),
                Some(Version::new(1, 0, 3)),
                Some(Version::new(1, 0, 5)),
                Some(Version::new(1, 3, 3)),
                Some(Version::new(2, 0, 3)),
            ],
        )]);
        let res =
            get_possible_realisations(&dependency, &versions_for_location, &location_resolver)
                .unwrap();
        assert_eq!(
            res,
            vec![
                ExpandedPackage {
                    location: exp_location_b.clone(),
                    version: Some(Version::new(1, 0, 3))
                },
                ExpandedPackage {
                    location: exp_location_b.clone(),
                    version: Some(Version::new(1, 0, 5))
                },
                ExpandedPackage {
                    location: exp_location_b.clone(),
                    version: Some(Version::new(1, 3, 3))
                }
            ]
        );
    }
}
