use std::collections::{HashMap, HashSet};

use tracing::debug;

use crate::quackpack::core::solver::solver_freeze::{SolverFreeze, SolverPackageFreeze};
use crate::quackpack::core::solver::solving::FoundSolution;
use crate::quackpack::core::{FeatureName, Manifest, PackageId, Selector};
use crate::util::extend::QpExtend;
use crate::{QuackResult, QuackResultContext, StrId, qp_bail_internal};

impl SolverFreeze {
    /// Generates a new freeze from the previous one and the solution found by solver.
    /// Trims the resulting freeze to contain only the necessary packages and features.
    /// The new freeze assumes that the main package is compiled with all its manifest-listed features.
    #[tracing::instrument(skip_all)]
    pub fn new_freeze(
        mut self,
        manifests: &HashMap<PackageId, Box<Manifest>>,
        solver_output: FoundSolution,
    ) -> QuackResult<Self> {
        self.add_solver_output(solver_output)?;
        let Some(main_pkg) = manifests.get(&self.main_pkg) else {
            qp_bail_internal!(
                "main package `{:?}` without a manifest: {manifests:#?}",
                self.main_pkg
            )
        };
        let main_features = main_pkg.features().all_features().keys().copied().collect();
        self.find_minimal_dep_solution(manifests, main_features)
    }

    /// Adds the solver output to the freeze.
    fn add_solver_output(self: &mut SolverFreeze, solver_output: FoundSolution) -> QuackResult<()> {
        for new_pkg in solver_output.new_packages {
            self.package_freezes
                .insert(new_pkg, SolverPackageFreeze::default());
        }
        for (pkg, new_features) in solver_output.new_features {
            let Some(pkg) = self.package_freezes.get_mut(&pkg) else {
                qp_bail_internal!(
                    "feature added to a package absent in the previous freeze and in the new packages set: {pkg:?}, {:#?}",
                    self.package_freezes
                )
            };
            pkg.features.extend(new_features);
        }
        for (dependency_edge, destination) in solver_output.new_edges {
            let Some(parent) = self.package_freezes.get_mut(&dependency_edge.parent) else {
                qp_bail_internal!(
                    "new dependency realization added to a package absent in the previous freeze and in the new packages set: {dependency_edge:?}, {:#?}",
                    self.package_freezes,
                )
            };
            parent.dependencies_realization.insert(
                dependency_edge.manifest_child_name,
                PackageId::new(dependency_edge.dep_identity, destination),
            );
        }
        Ok(())
    }

    /// Trims the freeze to contain the minimal sufficient set of packages and features.
    /// Assumes that the freeze is correct, but potentially unnecessary large.
    /// This can be used to either generate:
    ///     * the new freeze (main_pkg_features equals to all manifest-specified main package features).
    ///     * a compilation graph from the new freeze (main_pkg_features equals to features provided by the build command).
    #[tracing::instrument(skip_all)]
    pub fn find_minimal_dep_solution(
        self,
        manifests: &HashMap<PackageId, Box<Manifest>>,
        main_pkg_features: HashSet<FeatureName>,
    ) -> QuackResult<Self> {
        let mut new_package_freezes = HashMap::from([(
            self.main_pkg,
            SolverPackageFreeze {
                dependencies_realization: HashMap::new(),
                features: main_pkg_features,
            },
        )]);

        self.mark_children_as_necessary(self.main_pkg, manifests, &mut new_package_freezes)?;

        Ok(Self {
            package_freezes: new_package_freezes,
            ..self
        })
    }

    /// Recursive DFS-like helper for [`SolverFreeze::find_minimal_dep_solution`], to construct trimmed package to [`SolverPackageFreeze`] map.
    /// For a given package, it iterates over its dependencies and,
    /// for each realization, determines what features of the child are forced.
    /// If some new feature of the child is forced, the function calls itself, with that package as the base.
    fn mark_children_as_necessary(
        &self,
        base_pkg: PackageId,
        manifests: &HashMap<PackageId, Box<Manifest>>,
        new_pkg_freezes: &mut HashMap<PackageId, SolverPackageFreeze>,
    ) -> QuackResult<()> {
        let Some(base_manifest) = manifests.get(&base_pkg) else {
            qp_bail_internal!("freeze package `{base_pkg:?}` without a manifest: {manifests:#?}")
        };
        let base_pkg_features = self
            .current_pkg_features(new_pkg_freezes, base_pkg)?
            .clone();
        debug!(?base_pkg);

        for dependency in
            base_manifest
                .dependencies()
                .select(&Selector::EnabledBy(&HashSet::from_iter(
                    base_pkg_features.iter().copied(),
                )))
        {
            let dep_name = dependency.effective_name();
            let realization = self.get_realization(base_pkg, dep_name)?;
            let Some(dep_manifest) = manifests.get(&realization) else {
                qp_bail_internal!(
                    "freeze package `{realization:?}` with no manifest: {manifests:#?}"
                )
            };
            Self::add_realization(new_pkg_freezes, base_pkg, dep_name, realization)?;
            let enabled_features =
                dependency.enabled_features(&HashSet::from_iter(base_pkg_features.iter().copied()));
            let forced_features = dep_manifest
                .features()
                .expand_features(enabled_features.iter().copied())
                .with_context_internal(|| format!("unknown dependency feature while expanding features `{enabled_features:?}` in {dep_manifest:#?}"))?;
            let was_realization_present = new_pkg_freezes.contains_key(&realization);
            let child_new_freeze = new_pkg_freezes.entry(realization).or_default();

            if child_new_freeze
                .features
                .extend_and_get_diff_size(forced_features)
                > 0
                || !was_realization_present
            {
                // We trigger the recursive search, only if either:
                //  * `realization` was only now marked as necessary,
                //  * we marked some new features of `realization` as necessary.
                self.mark_children_as_necessary(realization, manifests, new_pkg_freezes)?;
            }
        }

        Ok(())
    }

    /// Helper for [`SolverFreeze::mark_children_as_necessary`].
    /// Finds with what features the package is currently listed in the new package freezes map.
    fn current_pkg_features<'a>(
        &self,
        new_pkg_freezes: &'a mut HashMap<PackageId, SolverPackageFreeze>,
        pkg: PackageId,
    ) -> QuackResult<&'a HashSet<FeatureName>> {
        let Some(pkg) = new_pkg_freezes.get(&pkg) else {
            qp_bail_internal!(
                "current package `{pkg:?}` does not appear in the new package freezes map: {new_pkg_freezes:#?}"
            )
        };
        Ok(&pkg.features)
    }

    /// Helper for [`SolverFreeze::mark_children_as_necessary`].
    /// Finds how a dependency is realized.
    fn get_realization(&self, pkg: PackageId, dep_name: StrId) -> QuackResult<PackageId> {
        let Some(pkg) = self.package_freezes.get(&pkg) else {
            qp_bail_internal!(
                "current package `{pkg:?}` does not appear in the freeze: {:#?}",
                self.package_freezes
            )
        };
        let Some(realization) = pkg.dependencies_realization.get(&dep_name) else {
            qp_bail_internal!("no realisation for `{dep_name}` in {:#?}", pkg)
        };
        Ok(*realization)
    }

    /// Helper for [`SolverFreeze::mark_children_as_necessary`].
    /// Adds the realization of the dependency to the package's freeze.
    fn add_realization(
        new_pkg_freezes: &mut HashMap<PackageId, SolverPackageFreeze>,
        pkg: PackageId,
        dep_name: StrId,
        realization: PackageId,
    ) -> QuackResult<()> {
        let Some(pkg) = new_pkg_freezes.get_mut(&pkg) else {
            qp_bail_internal!(
                "current package `{pkg:?}` does not appear in the new package freezes map: {new_pkg_freezes:#?}"
            )
        };
        pkg.dependencies_realization
            .entry(dep_name)
            .or_insert(realization);
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use std::collections::{HashMap, HashSet};
    use std::path::PathBuf;

    use tempfile::{TempDir, tempdir};

    use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
    use crate::quackpack::core::solver::dependency_edge::DependencyEdge;
    use crate::quackpack::core::solver::solver_freeze::{SolverFreeze, SolverPackageFreeze};
    use crate::quackpack::core::solver::solving::FoundSolution;
    use crate::quackpack::core::{FeatureName, PackageId, Version, parse_manifest};
    use crate::quackpack::util::to_url::ToUrl;
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
    fn add_solver_output() {
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: '1'

dependencies:
  b:
    version: '2'
    features: [xd]
  c:
    version: '3'
"#,
        );
        let (_dir_b, path_b) = prepare_manifest(
            r#"
metadata:
  name: b
  version: '2'

features:
  xd: []
"#,
        );
        let (_dir_c, path_c) = prepare_manifest(
            r#"
metadata:
  name: c
  version: '3'
"#,
        );
        let ctx = DuckContext::default();
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0.into_manifest();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0.into_manifest();
        let manifest_c = parse_manifest(&path_c, &ctx).unwrap().0.into_manifest();
        let registry_origin = FullOrigin::for_registry("http://localhost:9001".to_url().unwrap());
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let identity_c = FullIdentity::new("c".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let pkg_c = PackageId::new(identity_c, Version::new(3, 0, 0));
        let manifests = HashMap::from([
            (pkg_a, manifest_a),
            (pkg_b, manifest_b),
            (pkg_c, manifest_c),
        ]);
        let prev_a_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::from([(StrId::new("b"), pkg_b)]),
            features: HashSet::new(),
        };
        let prev_b_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::new(),
            features: HashSet::new(),
        };
        let prev_freeze = SolverFreeze {
            package_freezes: HashMap::from([(pkg_a, prev_a_freeze), (pkg_b, prev_b_freeze)]),
            main_pkg: pkg_a,
        };

        let solver_output = FoundSolution {
            new_packages: HashSet::from([pkg_c]),
            new_features: HashMap::from([(pkg_b, HashSet::from([FeatureName::new("xd")]))]),
            new_edges: HashMap::from([(
                DependencyEdge {
                    parent: pkg_a,
                    dep_identity: identity_c,
                    manifest_child_name: StrId::new("c"),
                },
                Version::new(3, 0, 0),
            )]),
        };

        let new_freeze = prev_freeze.new_freeze(&manifests, solver_output).unwrap();
        let freeze_a = new_freeze.package_freezes.get(&pkg_a).unwrap();
        let freeze_b = new_freeze.package_freezes.get(&pkg_b).unwrap();
        let freeze_c = new_freeze.package_freezes.get(&pkg_c).unwrap();
        assert_eq!(
            freeze_a.dependencies_realization,
            HashMap::from([(StrId::new("b"), pkg_b), (StrId::new("c"), pkg_c),])
        );
        assert!(freeze_a.features.is_empty());
        assert!(freeze_b.dependencies_realization.is_empty());
        assert_eq!(freeze_b.features, HashSet::from([FeatureName::new("xd")]));
        assert!(freeze_c.dependencies_realization.is_empty());
        assert!(freeze_c.features.is_empty());
    }

    #[test]
    fn trim_unneeded_package() {
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: '1'

dependencies:
  b:
    version: '2'
    features: [xd]
"#,
        );
        let (_dir_b, path_b) = prepare_manifest(
            r#"
metadata:
  name: b
  version: '2'

features:
  xd: []
"#,
        );
        let (_dir_c, path_c) = prepare_manifest(
            r#"
metadata:
  name: c
  version: '3'
"#,
        );
        let ctx = DuckContext::default();
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0.into_manifest();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0.into_manifest();
        let manifest_c = parse_manifest(&path_c, &ctx).unwrap().0.into_manifest();
        let registry_origin = FullOrigin::for_registry("http://localhost:9001".to_url().unwrap());
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let identity_c = FullIdentity::new("c".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let pkg_c = PackageId::new(identity_c, Version::new(3, 0, 0));
        let manifests = HashMap::from([
            (pkg_a, manifest_a),
            (pkg_b, manifest_b),
            (pkg_c, manifest_c),
        ]);
        let prev_a_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::from([
                (StrId::new("b"), pkg_b),
                (StrId::new("c"), pkg_c),
            ]),
            features: HashSet::new(),
        };
        let prev_b_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::new(),
            features: HashSet::new(),
        };
        let prev_c_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::new(),
            features: HashSet::new(),
        };
        let prev_freeze = SolverFreeze {
            package_freezes: HashMap::from([
                (pkg_a, prev_a_freeze),
                (pkg_b, prev_b_freeze),
                (pkg_c, prev_c_freeze),
            ]),
            main_pkg: pkg_a,
        };

        let solver_output = FoundSolution {
            new_packages: HashSet::new(),
            new_features: HashMap::new(),
            new_edges: HashMap::new(),
        };
        let new_freeze = prev_freeze.new_freeze(&manifests, solver_output).unwrap();
        let freeze_a = new_freeze.package_freezes.get(&pkg_a).unwrap();
        let freeze_b = new_freeze.package_freezes.get(&pkg_b).unwrap();
        assert!(!new_freeze.package_freezes.contains_key(&pkg_c));
        assert_eq!(
            freeze_a.dependencies_realization,
            HashMap::from([(StrId::new("b"), pkg_b)])
        );
        assert!(freeze_a.features.is_empty());
        assert!(freeze_b.dependencies_realization.is_empty());
        assert_eq!(freeze_b.features, HashSet::from([FeatureName::new("xd")]));
    }

    #[test]
    fn trim_unneeded_feature() {
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: '1'

dependencies:
  b:
    version: '2'
    features: [xd]
  c:
    version: '3'
"#,
        );
        let (_dir_b, path_b) = prepare_manifest(
            r#"
metadata:
  name: b
  version: '2'

features:
  xd: []
"#,
        );
        let (_dir_c, path_c) = prepare_manifest(
            r#"
metadata:
  name: c
  version: '3'
"#,
        );
        let ctx = DuckContext::default();
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0.into_manifest();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0.into_manifest();
        let manifest_c = parse_manifest(&path_c, &ctx).unwrap().0.into_manifest();
        let registry_origin = FullOrigin::for_registry("http://localhost:9001".to_url().unwrap());
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let identity_c = FullIdentity::new("c".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let pkg_c = PackageId::new(identity_c, Version::new(3, 0, 0));
        let manifests = HashMap::from([
            (pkg_a, manifest_a),
            (pkg_b, manifest_b),
            (pkg_c, manifest_c),
        ]);
        let prev_a_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::from([
                (StrId::new("b"), pkg_b),
                (StrId::new("c"), pkg_c),
            ]),
            features: HashSet::new(),
        };
        let prev_b_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::new(),
            features: HashSet::new(),
        };
        let prev_c_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::new(),
            features: HashSet::from([FeatureName::new("xdd")]),
        };
        let prev_freeze = SolverFreeze {
            package_freezes: HashMap::from([
                (pkg_a, prev_a_freeze),
                (pkg_b, prev_b_freeze),
                (pkg_c, prev_c_freeze),
            ]),
            main_pkg: pkg_a,
        };

        let solver_output = FoundSolution {
            new_packages: HashSet::new(),
            new_features: HashMap::new(),
            new_edges: HashMap::new(),
        };
        let new_freeze = prev_freeze.new_freeze(&manifests, solver_output).unwrap();
        let freeze_a = new_freeze.package_freezes.get(&pkg_a).unwrap();
        let freeze_b = new_freeze.package_freezes.get(&pkg_b).unwrap();
        let freeze_c = new_freeze.package_freezes.get(&pkg_c).unwrap();
        assert_eq!(
            freeze_a.dependencies_realization,
            HashMap::from([(StrId::new("b"), pkg_b), (StrId::new("c"), pkg_c),])
        );
        assert!(freeze_a.features.is_empty());
        assert!(freeze_b.dependencies_realization.is_empty());
        assert_eq!(freeze_b.features, HashSet::from([FeatureName::new("xd")]));
        assert!(freeze_c.dependencies_realization.is_empty());
        assert!(freeze_c.features.is_empty());
    }

    #[test]
    fn trim_nothing() {
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: '1'

dependencies:
  b:
    version: '2'
"#,
        );
        let (_dir_b, path_b) = prepare_manifest(
            r#"
metadata:
  name: b
  version: '2'

dependencies:
  c:
    version: '3'
"#,
        );
        let (_dir_c, path_c) = prepare_manifest(
            r#"
metadata:
  name: c
  version: '3'
"#,
        );
        let ctx = DuckContext::default();
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0.into_manifest();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0.into_manifest();
        let manifest_c = parse_manifest(&path_c, &ctx).unwrap().0.into_manifest();
        let registry_origin = FullOrigin::for_registry("http://localhost:9001".to_url().unwrap());
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let identity_c = FullIdentity::new("c".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let pkg_c = PackageId::new(identity_c, Version::new(3, 0, 0));
        let manifests = HashMap::from([
            (pkg_a, manifest_a),
            (pkg_b, manifest_b),
            (pkg_c, manifest_c),
        ]);
        let prev_a_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::from([(StrId::new("b"), pkg_b)]),
            features: HashSet::new(),
        };
        let prev_b_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::from([(StrId::new("c"), pkg_c)]),
            features: HashSet::new(),
        };
        let prev_c_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::new(),
            features: HashSet::new(),
        };
        let prev_freeze = SolverFreeze {
            package_freezes: HashMap::from([
                (pkg_a, prev_a_freeze),
                (pkg_b, prev_b_freeze),
                (pkg_c, prev_c_freeze),
            ]),
            main_pkg: pkg_a,
        };

        let solver_output = FoundSolution {
            new_packages: HashSet::new(),
            new_features: HashMap::new(),
            new_edges: HashMap::new(),
        };
        let new_freeze = prev_freeze.new_freeze(&manifests, solver_output).unwrap();
        let freeze_a = new_freeze.package_freezes.get(&pkg_a).unwrap();
        let freeze_b = new_freeze.package_freezes.get(&pkg_b).unwrap();
        let freeze_c = new_freeze.package_freezes.get(&pkg_c).unwrap();
        assert_eq!(
            freeze_a.dependencies_realization,
            HashMap::from([(StrId::new("b"), pkg_b)])
        );
        assert!(freeze_a.features.is_empty());
        assert_eq!(
            freeze_b.dependencies_realization,
            HashMap::from([(StrId::new("c"), pkg_c)])
        );
        assert!(freeze_b.features.is_empty());
        assert!(freeze_c.dependencies_realization.is_empty());
        assert!(freeze_c.features.is_empty());
    }

    #[test]
    fn root_feature() {
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: '1'

dependencies:
  b:
    version: '2'
    conditions:
      package-features: ['xd']

features:
  xd: []
"#,
        );
        let (_dir_b, path_b) = prepare_manifest(
            r#"
metadata:
  name: b
  version: '2'
"#,
        );
        let ctx = DuckContext::default();
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0.into_manifest();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0.into_manifest();
        let registry_origin = FullOrigin::for_registry("http://localhost:9001".to_url().unwrap());
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let manifests = HashMap::from([(pkg_a, manifest_a), (pkg_b, manifest_b)]);
        let prev_a_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::from([(StrId::new("b"), pkg_b)]),
            features: HashSet::new(),
        };
        let prev_b_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::new(),
            features: HashSet::new(),
        };
        let prev_freeze = SolverFreeze {
            package_freezes: HashMap::from([(pkg_a, prev_a_freeze), (pkg_b, prev_b_freeze)]),
            main_pkg: pkg_a,
        };

        let solver_output = FoundSolution {
            new_packages: HashSet::new(),
            new_features: HashMap::new(),
            new_edges: HashMap::new(),
        };
        let new_freeze = prev_freeze.new_freeze(&manifests, solver_output).unwrap();
        let freeze_a = new_freeze.package_freezes.get(&pkg_a).unwrap();
        let freeze_b = new_freeze.package_freezes.get(&pkg_b).unwrap();
        assert_eq!(
            freeze_a.dependencies_realization,
            HashMap::from([(StrId::new("b"), pkg_b)])
        );
        assert_eq!(freeze_a.features, ["xd".into()].into());
        assert!(freeze_b.dependencies_realization.is_empty());
        assert!(freeze_b.features.is_empty());
    }
}
