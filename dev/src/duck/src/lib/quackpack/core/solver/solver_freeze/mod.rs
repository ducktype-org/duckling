mod freeze_diagnosis;
mod new_freeze_generation;

use std::collections::{HashMap, HashSet};

use tracing::debug;

use crate::quackpack::core::identity::{Identity, Kind, realization_and_manifest_to_identity};
use crate::quackpack::core::simple_identity::realization_and_manifest_to_simple_identity;
use crate::quackpack::core::solver::types_common::{ExpandedLocation, ExpandedPackage};
use crate::quackpack::core::storage::freeze::{FreezePackage, RootPackage, VenvFreeze};
use crate::quackpack::core::{FeatureName, Manifest};
use crate::{QuackResult, QuackResultContext, StrId};

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SolverFreeze {
    pub package_freezes: HashMap<ExpandedPackage, SolverPackageFreeze>,
    pub main_pkg: ExpandedPackage,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SolverPackageFreeze {
    pub dependencies_realization: HashMap<StrId, ExpandedPackage>,
    pub features: HashSet<FeatureName>,
}

impl SolverPackageFreeze {
    pub fn new() -> Self {
        Self {
            dependencies_realization: HashMap::new(),
            features: HashSet::new(),
        }
    }
}

impl Default for SolverPackageFreeze {
    fn default() -> Self {
        Self::new()
    }
}

fn identity_to_expanded_loc(identity: &Identity) -> ExpandedLocation {
    let url = identity.origin().url();
    match identity.origin().kind() {
        Kind::Registry => ExpandedLocation::Registry {
            url,
            real_name: identity.name(),
        },
        Kind::Git { commit } => ExpandedLocation::Git { url, commit },
        Kind::Local => ExpandedLocation::Local { absolute_path: url },
    }
}

impl SolverFreeze {
    // @TODO: #2076 Fix issues with storage's freeze.
    #[tracing::instrument(skip_all)]
    pub fn try_from_venv_freeze(root: ExpandedPackage, value: &VenvFreeze) -> QuackResult<Self> {
        debug!(root = ?root, freeze = ?value);
        let mut expanded_pkgs_by_name = HashMap::new();
        for pkg_freeze in value.dependencies() {
            let pkg = ExpandedPackage {
                location: identity_to_expanded_loc(pkg_freeze.identity()),
                version: if pkg_freeze.identity().origin().kind().is_registry() {
                    Some(pkg_freeze.version())
                } else {
                    None
                },
            };
            expanded_pkgs_by_name.insert(pkg_freeze.name(), pkg);
        }
        let mut pkg_freezes = HashMap::new();
        for pkg_freeze in value.dependencies() {
            let pkg = expanded_pkgs_by_name
                .get(&pkg_freeze.name())
                .context_internal("No package with given name")?;
            let mut dependencies = HashMap::new();
            for dep in pkg_freeze.dependencies() {
                let realization = expanded_pkgs_by_name
                    .get(&dep.name())
                    .context_internal("No package with given name")?;
                dependencies.insert(dep.name(), *realization);
            }
            pkg_freezes.insert(
                *pkg,
                SolverPackageFreeze {
                    dependencies_realization: dependencies,
                    features: pkg_freeze.features().iter().copied().collect(),
                },
            );
        }

        let mut main_dependencies = HashMap::new();
        for dep in value.root().dependencies() {
            let realization = expanded_pkgs_by_name
                .get(&dep.name())
                .context_internal("No package with given name")?;
            if *realization != root {
                main_dependencies.insert(dep.name(), *realization);
            }
        }
        pkg_freezes.insert(
            root,
            SolverPackageFreeze {
                dependencies_realization: main_dependencies,
                features: value.root().features().iter().copied().collect(),
            },
        );

        Ok(Self {
            main_pkg: root,
            package_freezes: pkg_freezes,
        })
    }
}

impl SolverFreeze {
    #[tracing::instrument(skip_all)]
    pub fn empty_with_root(root: ExpandedPackage) -> QuackResult<Self> {
        debug!(?root);
        Ok(Self {
            main_pkg: root,
            package_freezes: [(root, SolverPackageFreeze::default())].into(),
        })
    }

    #[tracing::instrument(skip_all)]
    pub fn generate_storage_freeze(
        self,
        manifests: &HashMap<ExpandedPackage, Box<Manifest>>,
    ) -> QuackResult<VenvFreeze> {
        debug!(root = ?self.main_pkg, freeze = ?self.package_freezes);
        let mut pkg_freezes = vec![];
        let root_freeze = self
            .package_freezes
            .get(&self.main_pkg)
            .context_internal("No main freeze")?
            .clone();
        for (pkg, freeze) in self.package_freezes {
            if pkg == self.main_pkg {
                continue;
            }
            let package_manifest = manifests.get(&pkg).context_internal("No main manifest")?;
            let mut dependencies = vec![];
            for (_, realization) in freeze.dependencies_realization {
                let realization_manifest = manifests
                    .get(&realization)
                    .context_internal("No manifest for realization")?;
                dependencies.push(realization_and_manifest_to_simple_identity(
                    realization,
                    realization_manifest,
                ));
            }
            pkg_freezes.push(FreezePackage::new(
                realization_and_manifest_to_identity(pkg, package_manifest),
                package_manifest.version(),
                freeze.features.into_iter().collect::<Vec<_>>(),
                dependencies,
            ));
        }
        let mut root_deps = vec![];
        let root_manifest = manifests
            .get(&self.main_pkg)
            .context_internal("No main manifest")?;
        for (_, realization) in root_freeze.dependencies_realization {
            let realization_manifest = manifests
                .get(&realization)
                .context_internal("No manifest for realization")?;
            root_deps.push(realization_and_manifest_to_simple_identity(
                realization,
                realization_manifest,
            ));
        }
        let root = RootPackage::new(
            root_manifest.name(),
            root_manifest.version(),
            root_freeze.features.into_iter().collect::<Vec<_>>(),
            root_deps,
        );
        Ok(VenvFreeze::new(root, pkg_freezes))
    }
}

#[cfg(test)]
mod test {
    use std::path::PathBuf;

    use tempfile::{TempDir, tempdir};

    use super::*;
    use crate::DuckContext;
    use crate::quackpack::core::identity::Origin;
    use crate::quackpack::core::simple_identity::{SimpleIdentity, SimpleOrigin};
    use crate::quackpack::core::solver::types_common::ExpandedLocation;
    use crate::quackpack::core::{PackageLoader, parse_manifest};
    use crate::quackpack::util::to_url::ToUrl;
    use crate::util::path_ops_ext::PathOpsExt;

    fn prepare_manifest(contents: &str) -> (TempDir, PathBuf) {
        let dir = tempdir().unwrap();
        let manifest = dir.path().join(PackageLoader::MANIFEST_NAME);
        manifest.touch().unwrap();
        manifest.write(contents).unwrap();
        dir.path().try_fsync_dir().unwrap();
        (dir, manifest)
    }

    #[test]
    fn storage_to_solver_freeze() {
        let loc_a = ExpandedLocation::Registry {
            url: "https://example.net".to_url().unwrap().into(),
            real_name: "a".into(),
        };
        let loc_b = ExpandedLocation::Local {
            absolute_path: PathBuf::from("/xdd").to_url().unwrap().into(),
        };
        let pkg_a = ExpandedPackage {
            location: loc_a,
            version: Some(1.into()),
        };
        let pkg_b = ExpandedPackage {
            location: loc_b,
            version: None,
        };
        let freeze_pkg_a = FreezePackage::new(
            Identity::new(
                "a".into(),
                Origin::for_registry("https://example.net".to_url().unwrap()),
            ),
            1.into(),
            vec!["f_a".into()],
            vec![],
        );
        let freeze_pkg_b = FreezePackage::new(
            Identity::new(
                "b".into(),
                Origin::for_local(&PathBuf::from("/xdd")).unwrap(),
            ),
            2.into(),
            vec!["f_b1".into(), "f_b2".into()],
            vec![SimpleIdentity::new(
                "a".into(),
                SimpleOrigin::for_registry("https://example.net".to_url().unwrap()),
            )],
        );
        let root = RootPackage::new(
            "root".into(),
            3.into(),
            vec!["f_root".into()],
            vec![
                SimpleIdentity::new(
                    "a".into(),
                    SimpleOrigin::for_registry("https://example.net".to_url().unwrap()),
                ),
                SimpleIdentity::new(
                    "b".into(),
                    SimpleOrigin::for_local(&PathBuf::from("/xdd")).unwrap(),
                ),
            ],
        );
        let root_pkg = ExpandedPackage {
            location: ExpandedLocation::Local {
                absolute_path: PathBuf::from("/").to_url().unwrap().into(),
            },
            version: None,
        };
        let storage_freeze = VenvFreeze::new(root, vec![freeze_pkg_a, freeze_pkg_b]);
        let solver_freeze = SolverFreeze::try_from_venv_freeze(root_pkg, &storage_freeze).unwrap();
        assert_eq!(solver_freeze.main_pkg, root_pkg);
        assert_eq!(
            solver_freeze.package_freezes,
            HashMap::from([
                (
                    root_pkg,
                    SolverPackageFreeze {
                        dependencies_realization: [("a".into(), pkg_a), ("b".into(), pkg_b)].into(),
                        features: ["f_root".into()].into(),
                    }
                ),
                (
                    pkg_a,
                    SolverPackageFreeze {
                        dependencies_realization: HashMap::new(),
                        features: ["f_a".into()].into(),
                    }
                ),
                (
                    pkg_b,
                    SolverPackageFreeze {
                        dependencies_realization: [("a".into(), pkg_a)].into(),
                        features: ["f_b1".into(), "f_b2".into()].into(),
                    }
                ),
            ])
        )
    }

    #[test]
    fn solver_to_storage_freeze() {
        let (_dir_root, path_root) = prepare_manifest(
            r#"
metadata:
  name: root
  version: '3'

dependencies:
  alias_a:
    source:
      registry_url: https://example.net
      name: a
    version: '1'
  alias_b:
    source:
      path: ./sialalala

features:
  f_root: []
"#,
        );
        let (_dir_b, path_b) = prepare_manifest(
            r#"
metadata:
  name: b
  version: '2'

dependencies:
  a:
    source:
      registry_url: https://example.net
    version: '1'

features:
  f_b: []
"#,
        );
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: '1'

features:
  f_a: []
"#,
        );
        let ctx = DuckContext::default();
        let manifest_root = parse_manifest(&path_root, &ctx).unwrap();
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let exp_location_root = ExpandedLocation::Local {
            absolute_path: PathBuf::from("/root_path").to_url().unwrap().into(),
        };
        let exp_location_a = ExpandedLocation::Registry {
            url: "https://example.net".to_url().unwrap().into(),
            real_name: StrId::from("a"),
        };
        let exp_location_b = ExpandedLocation::Local {
            absolute_path: PathBuf::from("/sialalala").to_url().unwrap().into(),
        };
        let exp_pkg_root = ExpandedPackage {
            location: exp_location_root,
            version: None,
        };
        let exp_pkg_a = ExpandedPackage {
            location: exp_location_a,
            version: Some(1.into()),
        };
        let exp_pkg_b = ExpandedPackage {
            location: exp_location_b,
            version: None,
        };
        let manifests = HashMap::from([
            (exp_pkg_root, Box::new(manifest_root.manifest().clone())),
            (exp_pkg_a, Box::new(manifest_a.manifest().clone())),
            (exp_pkg_b, Box::new(manifest_b.manifest().clone())),
        ]);
        let solver_freeze = SolverFreeze {
            main_pkg: exp_pkg_root,
            package_freezes: HashMap::from([
                (
                    exp_pkg_root,
                    SolverPackageFreeze {
                        dependencies_realization: [
                            ("alias_a".into(), exp_pkg_a),
                            ("b".into(), exp_pkg_b),
                        ]
                        .into(),
                        features: ["f_root".into()].into(),
                    },
                ),
                (
                    exp_pkg_b,
                    SolverPackageFreeze {
                        dependencies_realization: [("a".into(), exp_pkg_a)].into(),
                        features: ["f_b".into()].into(),
                    },
                ),
                (
                    exp_pkg_a,
                    SolverPackageFreeze {
                        dependencies_realization: [].into(),
                        features: ["f_a".into()].into(),
                    },
                ),
            ]),
        };
        let storage_freeze = solver_freeze.generate_storage_freeze(&manifests).unwrap();
        let root_deps: HashSet<SimpleIdentity> = storage_freeze
            .root()
            .dependencies()
            .iter()
            .copied()
            .collect();
        assert_eq!(
            root_deps,
            HashSet::from([
                SimpleIdentity::new(
                    "a".into(),
                    SimpleOrigin::for_registry("https://example.net".to_url().unwrap()),
                ),
                SimpleIdentity::new(
                    "b".into(),
                    SimpleOrigin::for_local(&PathBuf::from("/sialalala")).unwrap(),
                ),
            ])
        );
        assert_eq!(storage_freeze.root().features(), [StrId::new("f_root")]);
        assert_eq!(storage_freeze.root().name(), StrId::new("root"));
        assert_eq!(storage_freeze.root().version(), 3.into());

        let pkg_freeze_a = FreezePackage::new(
            Identity::new(
                "a".into(),
                Origin::for_registry("https://example.net".to_url().unwrap()),
            ),
            1.into(),
            vec!["f_a".into()],
            vec![],
        );
        let pkg_freeze_b = FreezePackage::new(
            Identity::new(
                "b".into(),
                Origin::for_local(&PathBuf::from("/sialalala")).unwrap(),
            ),
            2.into(),
            vec!["f_b".into()],
            vec![SimpleIdentity::new(
                "a".into(),
                SimpleOrigin::for_registry("https://example.net".to_url().unwrap()),
            )],
        );
        assert_eq!(storage_freeze.dependencies().len(), 2);
        if storage_freeze.dependencies()[0] == pkg_freeze_a {
            assert_eq!(storage_freeze.dependencies()[1], pkg_freeze_b)
        } else {
            assert_eq!(storage_freeze.dependencies()[0], pkg_freeze_b);
            assert_eq!(storage_freeze.dependencies()[1], pkg_freeze_a)
        }
    }
}
