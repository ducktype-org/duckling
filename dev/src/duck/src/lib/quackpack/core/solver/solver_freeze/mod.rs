mod freeze_diagnosis;
mod new_freeze_generation;

use std::collections::{HashMap, HashSet};

use tracing::debug;

use crate::quackpack::core::full_identity::FullIdentity;
use crate::quackpack::core::storage::freeze::{FreezePackage, RootPackage, VenvFreeze};
use crate::quackpack::core::FeatureName;
use crate::quackpack::util::with_version::WithVersion;
use crate::{QuackResult, QuackResultContext, StrId};

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SolverFreeze {
    pub package_freezes: HashMap<WithVersion<FullIdentity>, SolverPackageFreeze>,
    pub main_pkg: WithVersion<FullIdentity>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SolverPackageFreeze {
    pub dependencies_realization: HashMap<StrId, WithVersion<FullIdentity>>,
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

impl SolverFreeze {
    // @TODO: #2076 Fix issues with storage's freeze.
    #[tracing::instrument(skip_all)]
    pub fn try_from_venv_freeze(root: WithVersion<FullIdentity>, value: &VenvFreeze) -> QuackResult<Self> {
        debug!(root = ?root, freeze = ?value);
        let mut expanded_pkgs_by_name = HashMap::new();
        for pkg_freeze in value.dependencies() {
            let pkg = WithVersion::new(*pkg_freeze.identity(), pkg_freeze.version());
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
    pub fn empty_with_root(root: WithVersion<FullIdentity>) -> QuackResult<Self> {
        debug!(?root);
        Ok(Self {
            main_pkg: root,
            package_freezes: [(root, SolverPackageFreeze::default())].into(),
        })
    }

    #[tracing::instrument(skip_all)]
    pub fn generate_storage_freeze(self) -> QuackResult<VenvFreeze> {
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
            let mut dependencies = vec![];
            for (_, realization) in freeze.dependencies_realization {
                dependencies.push((*realization.value()).into());
            }
            pkg_freezes.push(FreezePackage::new(
                *pkg.value(),
                pkg.version(),
                freeze.features.into_iter().collect::<Vec<_>>(),
                dependencies,
            ));
        }
        let mut root_deps = vec![];
        for (_, realization) in root_freeze.dependencies_realization {
            root_deps.push((*realization.value()).into());
        }
        let root = RootPackage::new(
            self.main_pkg.value().name(),
            self.main_pkg.version(),
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
    use crate::quackpack::core::full_identity::FullOrigin;
use crate::quackpack::core::identity::{Identity, Origin};
use crate::quackpack::core::{PackageLoader, Version};
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
        let origin_a = FullOrigin::for_registry("https://example.net".to_url().unwrap());
        let origin_b = FullOrigin::for_local(&PathBuf::from("/xdd")).unwrap();
        let identity_a = FullIdentity::new("a".into(), origin_a);
        let identity_b = FullIdentity::new("b".into(), origin_b);
        let pkg_a = WithVersion::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = WithVersion::new(identity_b, Version::new(2, 0, 0));
        let freeze_pkg_a = FreezePackage::new(
            FullIdentity::new(
                "a".into(),
                FullOrigin::for_registry("https://example.net".to_url().unwrap()),
            ),
            1.into(),
            vec!["f_a".into()],
            vec![],
        );
        let freeze_pkg_b = FreezePackage::new(
            FullIdentity::new(
                "b".into(),
                FullOrigin::for_local(&PathBuf::from("/xdd")).unwrap(),
            ),
            2.into(),
            vec!["f_b1".into(), "f_b2".into()],
            vec![Identity::new(
                "a".into(),
                Origin::for_registry("https://example.net".to_url().unwrap()),
            )],
        );
        let root = RootPackage::new(
            "root".into(),
            3.into(),
            vec!["f_root".into()],
            vec![
                Identity::new(
                    "a".into(),
                    Origin::for_registry("https://example.net".to_url().unwrap()),
                ),
                Identity::new(
                    "b".into(),
                    Origin::for_local(&PathBuf::from("/xdd")).unwrap(),
                ),
            ],
        );
        let origin_root = FullOrigin::for_local(&PathBuf::from("/")).unwrap();
        let identity_root = FullIdentity::new("root".into(), origin_root);
        let pkg_root = WithVersion::new(identity_root, Version::new(3, 0, 0));
        let storage_freeze = VenvFreeze::new(root, vec![freeze_pkg_a, freeze_pkg_b]);
        let solver_freeze = SolverFreeze::try_from_venv_freeze(pkg_root, &storage_freeze).unwrap();
        assert_eq!(solver_freeze.main_pkg, pkg_root);
        assert_eq!(
            solver_freeze.package_freezes,
            HashMap::from([
                (
                    pkg_root,
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
        let origin_root = FullOrigin::for_local( &PathBuf::from("/root_path")).unwrap();
        let origin_a = FullOrigin::for_registry("https://example.net".to_url().unwrap());
        // cSpell:disable-next-line
        let origin_b = FullOrigin::for_local(&PathBuf::from("/sialalala")).unwrap();
        let identity_root = FullIdentity::new("root".into(), origin_root);
        let identity_a = FullIdentity::new("a".into(), origin_a);
        let identity_b = FullIdentity::new("b".into(), origin_b);
        let pkg_root = WithVersion::new(identity_root, Version::new(3, 0, 0));
        let pkg_a = WithVersion::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = WithVersion::new(identity_b, Version::new(2, 0, 0));
        let solver_freeze = SolverFreeze {
            main_pkg: pkg_root,
            package_freezes: HashMap::from([
                (
                    pkg_root,
                    SolverPackageFreeze {
                        dependencies_realization: [
                            ("alias_a".into(), pkg_a),
                            ("b".into(), pkg_b),
                        ]
                        .into(),
                        features: ["f_root".into()].into(),
                    },
                ),
                (
                    pkg_b,
                    SolverPackageFreeze {
                        dependencies_realization: [("a".into(), pkg_a)].into(),
                        features: ["f_b".into()].into(),
                    },
                ),
                (
                    pkg_a,
                    SolverPackageFreeze {
                        dependencies_realization: [].into(),
                        features: ["f_a".into()].into(),
                    },
                ),
            ]),
        };
        let storage_freeze = solver_freeze.generate_storage_freeze().unwrap();
        let root_deps: HashSet<Identity> = storage_freeze
            .root()
            .dependencies()
            .iter()
            .copied()
            .collect();
        assert_eq!(
            root_deps,
            HashSet::from([
                Identity::new(
                    "a".into(),
                    Origin::for_registry("https://example.net".to_url().unwrap()),
                ),
                Identity::new(
                    "b".into(),
                    Origin::for_local(&PathBuf::from("/sialalala")).unwrap(),
                ),
            ])
        );
        assert_eq!(storage_freeze.root().features(), [StrId::new("f_root")]);
        assert_eq!(storage_freeze.root().name(), StrId::new("root"));
        assert_eq!(storage_freeze.root().version(), 3.into());

        let pkg_freeze_a = FreezePackage::new(
            FullIdentity::new(
                "a".into(),
                FullOrigin::for_registry("https://example.net".to_url().unwrap()),
            ),
            1.into(),
            vec!["f_a".into()],
            vec![],
        );
        let pkg_freeze_b = FreezePackage::new(
            FullIdentity::new(
                "b".into(),
                FullOrigin::for_local(&PathBuf::from("/sialalala")).unwrap(),
            ),
            2.into(),
            vec!["f_b".into()],
            vec![Identity::new(
                "a".into(),
                Origin::for_registry("https://example.net".to_url().unwrap()),
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
