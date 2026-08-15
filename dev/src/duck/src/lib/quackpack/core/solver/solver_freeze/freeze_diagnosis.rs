use std::cell::RefCell;
use std::collections::{HashMap, HashSet};

use tracing::debug;

use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::solver::gathering::fetch_types::{FetchResponse, FetchSuccess};
use crate::quackpack::core::solver::gathering::gatherer::Gatherer;
use crate::quackpack::core::solver::git_access::GitAccess;
use crate::quackpack::core::solver::solver_freeze::{SolverFreeze, SolverPackageFreeze};
use crate::quackpack::core::{Dependency, FeatureName, Manifest, PackageId};
use crate::util::error::ErrorsLogger;
use crate::{QuackResult, qp_bail_internal};

impl SolverFreeze {
    /// Fetches manifests of the packages mentioned in the freeze (but not the root package),
    /// to later check whether their dependencies are still satisfied inside the freeze.
    pub async fn get_prev_freeze_manifests<Access: GitAccess>(
        &self,
        gatherer: &Gatherer<'_, '_, Access>,
    ) -> QuackResult<HashMap<PackageId, Box<Manifest>>> {
        let mut results = vec![];
        let errors = RefCell::new(ErrorsLogger::default());
        for pkg in self.package_freezes.keys() {
            if *pkg == self.main_pkg {
                continue;
            }
            if let Ok(request) = pkg.create_manifest_request() {
                results.push(gatherer.fetch(request, &errors).await);
            }
        }
        let mut manifests = HashMap::new();
        for fetch_response in results {
            let fetch_response = fetch_response?;
            if let FetchResponse::Success(success_response) = fetch_response {
                match success_response {
                    FetchSuccess::Pinned(pinned_success) => {
                        manifests.insert(
                            pinned_success.answer_package,
                            pinned_success.fetched_manifest,
                        );
                    }
                    FetchSuccess::NotPinned(not_pinned_success) => {
                        manifests.extend(not_pinned_success.fetched_manifests);
                    }
                }
            }
        }
        Ok(manifests)
    }

    /// Finds the maximal subset of the freeze which is a correct dependency resolution,
    /// with a relaxation that main package dependencies may not be realised.
    ///
    /// This is done by firstly finding which freeze entries are not immediately flawed
    /// (manifest matches the package and each realization from the freeze really realizes its manifest counterpart).
    /// Then the information about being flawed is propagated upwards (if child is flawed then so is parent who depends on it).
    ///
    /// Should be used as a preprocessing tool, before the freeze is passed through the solver.
    #[tracing::instrument(skip_all)]
    pub async fn find_maximal_correct_dep_solution(
        mut self,
        manifests: &HashMap<PackageId, Box<Manifest>>,
        fetcher: &Fetcher<'_>,
    ) -> QuackResult<(Self, bool)> {
        self.retain_not_flawed_pkgs(manifests, fetcher).await?;
        self.substitute_root_pkg(manifests, fetcher).await
    }

    /// Helper for [`Self::find_maximal_correct_dep_solution`]
    /// Retains freezes of the old root package (it is later substituted anyway)
    /// and all the packages which have dependencies transitively satisfied.
    async fn retain_not_flawed_pkgs(
        &mut self,
        manifests: &HashMap<PackageId, Box<Manifest>>,
        fetcher: &Fetcher<'_>,
    ) -> QuackResult<()> {
        let mut still_satisfied_pkgs = self.still_satisfied_pkgs(manifests, fetcher).await?;
        let reversed_graph = self.reversed_dependency_graph();
        let mut visited = HashSet::new();
        for pkg in reversed_graph.keys() {
            if !still_satisfied_pkgs.contains(pkg) {
                Self::flawed_pkgs_dfs(
                    *pkg,
                    &reversed_graph,
                    &mut visited,
                    &mut still_satisfied_pkgs,
                );
            }
        }
        self.package_freezes.retain(|pkg, _| {
            let is_root_package = *pkg == self.main_pkg;
            let is_satisfied = still_satisfied_pkgs.contains(pkg);
            debug!(?pkg, %is_root_package, %is_satisfied);
            is_satisfied || is_root_package
        });
        Ok(())
    }

    /// Helper for [`Self::find_maximal_correct_dep_solution`].
    /// Finds which packages from the freeze are not immediately flawed:
    ///     * we were able to obtain their manifests and the manifests agrees with the package,
    ///     * all manifest dependencies are satisfied by appropriate freeze-written realizations.
    async fn still_satisfied_pkgs(
        &self,
        manifests: &HashMap<PackageId, Box<Manifest>>,
        fetcher: &Fetcher<'_>,
    ) -> QuackResult<HashSet<PackageId>> {
        let mut still_satisfied_pkgs = HashSet::new();
        for (pkg, freeze) in self.package_freezes.iter() {
            let Some(manifest) = Self::get_and_check_manifest(*pkg, manifests, &freeze.features)
            else {
                continue;
            };
            let mut is_every_dep_satisfied = true;
            for dep in manifest.dependencies().all_dependencies().iter() {
                let Some(realization) = freeze.dependencies_realization.get(&dep.effective_name())
                else {
                    is_every_dep_satisfied = false;
                    break;
                };
                let Some(realization_freeze) = self.package_freezes.get(realization) else {
                    is_every_dep_satisfied = false;
                    break;
                };
                is_every_dep_satisfied &= Self::check_if_dep_is_satisfied(
                    freeze,
                    *realization,
                    realization_freeze,
                    dep,
                    fetcher,
                )
                .await?;
            }
            if is_every_dep_satisfied {
                still_satisfied_pkgs.insert(*pkg);
            }
        }
        Ok(still_satisfied_pkgs)
    }

    /// Helper for [`Self::still_satisfied_pkgs`].
    /// Checks if we have a manifest for a package and whether its coherent with the freeze, meaning:
    ///     * name in the manifest agrees with the name in the freeze,
    ///     * all its freeze-present features still appear in the manifest,
    ///     * freeze-present features are expansion-closed,
    ///     * freeze-present version equals manifest version.
    fn get_and_check_manifest<'a>(
        pkg: PackageId,
        manifests: &'a HashMap<PackageId, Box<Manifest>>,
        features: &HashSet<FeatureName>,
    ) -> Option<&'a Manifest> {
        let manifest = manifests.get(&pkg)?;
        if manifest.name() != pkg.name() {
            return None;
        }
        if features
            .iter()
            .any(|f| !manifest.features().has_feature(*f))
        {
            return None;
        }
        if manifest
            .features()
            .expand_features(features.iter().copied())
            .expect("We checked that freeze features occur in manifest")
            != *features
        {
            return None;
        }
        if manifest.version() == pkg.version() {
            Some(manifest)
        } else {
            None
        }
    }

    /// Helper for [`Self::still_satisfied_pkgs`].
    /// Checks if a particular dependency is satisfied.
    async fn check_if_dep_is_satisfied(
        freeze: &SolverPackageFreeze,
        realization: PackageId,
        realization_freeze: &SolverPackageFreeze,
        dep: &Dependency,
        fetcher: &Fetcher<'_>,
    ) -> QuackResult<bool> {
        if !realization.still_satisfies_dep(dep, fetcher).await? {
            return Ok(false);
        }
        let forced_child_features =
            dep.enabled_features(Vec::from_iter(freeze.features.iter().copied()));
        // Check whether features forced by the dependency on the realisation are all present in its freeze.
        // We do not have to expand them, since we have already checked in `get_manifest_and_check_features_exist`,
        // that freeze-present features are closed under expansion.
        Ok(realization_freeze
            .features
            .is_superset(&HashSet::from_iter(forced_child_features)))
    }

    /// Helper for [`Self::retain_not_flawed_pkgs`].
    /// Expands the notion of a flawed package in a dfs-like manner, by applying a rule that
    /// if for some package and its dependency, the realization is flawed, the package is as well.
    fn flawed_pkgs_dfs(
        cur_pkg: PackageId,
        graph: &HashMap<PackageId, Vec<PackageId>>,
        visited: &mut HashSet<PackageId>,
        still_satisfied_pkgs: &mut HashSet<PackageId>,
    ) {
        visited.insert(cur_pkg);
        still_satisfied_pkgs.remove(&cur_pkg);
        let Some(edges) = graph.get(&cur_pkg) else {
            return;
        };
        for parent in edges {
            if !visited.contains(parent) {
                Self::flawed_pkgs_dfs(*parent, graph, visited, still_satisfied_pkgs);
            }
        }
    }

    /// Helper for [`Self::retain_not_flawed_pkgs`].
    /// Generates the graph used by [`Self::flawed_pkgs_dfs`].
    fn reversed_dependency_graph(&self) -> HashMap<PackageId, Vec<PackageId>> {
        let mut reversed_graph: HashMap<PackageId, Vec<PackageId>> = HashMap::new();
        for (pkg, freeze) in self.package_freezes.iter() {
            for realization in freeze.dependencies_realization.values() {
                reversed_graph.entry(*realization).or_default().push(*pkg);
            }
        }
        reversed_graph
    }

    /// Helper for [`Self::find_maximal_correct_dep_solution`].
    /// In the freeze, removes the old root and substitutes its package freeze as the new root's package freeze.
    /// Keeps only still satisfied realizations from the old root's package freeze.
    async fn substitute_root_pkg(
        mut self,
        manifests: &HashMap<PackageId, Box<Manifest>>,
        fetcher: &Fetcher<'_>,
    ) -> QuackResult<(Self, bool)> {
        let Some(main_pkg_freeze) = self.package_freezes.get(&self.main_pkg) else {
            qp_bail_internal!("main package was not put into package freezes: {self:#?}")
        };
        let Some(main_manifest) = manifests.get(&self.main_pkg) else {
            qp_bail_internal!("main package manifest not provided: {self:#?} {manifests:#?}")
        };
        // Check which main package dependencies are still satisfied.
        let mut still_satisfied_root_deps = HashSet::new();
        for (alias, realization) in main_pkg_freeze.dependencies_realization.iter() {
            if let Some(dependency) = main_manifest.dependencies().get_by_effective_name(*alias)
                && let Some(realization_freeze) = self.package_freezes.get(realization)
                && Self::check_if_dep_is_satisfied(
                    main_pkg_freeze,
                    *realization,
                    realization_freeze,
                    dependency,
                    fetcher,
                )
                .await?
            {
                still_satisfied_root_deps.insert(*alias);
            }
        }
        // Leave only still satisfied dependencies.
        let Some(main_pkg_freeze) = self.package_freezes.get_mut(&self.main_pkg) else {
            qp_bail_internal!("main package manifest not provided: {self:#?}")
        };
        main_pkg_freeze
            .dependencies_realization
            .retain(|alias, _| still_satisfied_root_deps.contains(alias));
        let all_main_pkg_deps_satisfied = main_manifest
            .dependencies()
            .all_dependencies()
            .iter()
            .all(|dep| {
                main_pkg_freeze
                    .dependencies_realization
                    .contains_key(&dep.effective_name())
            });
        // Change the previous main package to the new root package.
        let Some(main_pkg_freeze) = self.package_freezes.remove(&self.main_pkg) else {
            qp_bail_internal!("main package manifest not provided: {self:#?}")
        };
        self.package_freezes.insert(self.main_pkg, main_pkg_freeze);
        Ok((self, all_main_pkg_deps_satisfied))
    }
}

#[cfg(test)]
mod test {
    use std::collections::{HashMap, HashSet};
    use std::path::PathBuf;

    use futures::executor::block_on;
    use tempfile::{TempDir, tempdir};

    use crate::quackpack::core::fetcher::Fetcher;
    use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
    use crate::quackpack::core::solver::solver_freeze::{SolverFreeze, SolverPackageFreeze};
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
    fn nothing_to_do() {
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: '1'

dependencies:
  b:
    version: '2'
    features:
    - xd:
        package_features: [foo]

features:
  foo: []
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
        let ctx = DuckContext::default();
        let fetcher = Fetcher::new(&ctx).unwrap();
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let registry_origin = FullOrigin::for_registry("http://localhost:9001".to_url().unwrap());
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let manifests = HashMap::from([
            (pkg_a, Box::new(manifest_a.into_manifest())),
            (pkg_b, Box::new(manifest_b.into_manifest())),
        ]);
        let prev_a_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::from([(StrId::new("b"), pkg_b)]),
            features: HashSet::from([FeatureName::new("foo")]),
        };
        let prev_b_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::new(),
            features: HashSet::from([FeatureName::new("xd")]),
        };
        let prev_freeze = SolverFreeze {
            package_freezes: HashMap::from([(pkg_a, prev_a_freeze), (pkg_b, prev_b_freeze)]),
            main_pkg: pkg_a,
        };
        let (new_freeze, _) = block_on(
            prev_freeze
                .clone()
                .find_maximal_correct_dep_solution(&manifests, &fetcher),
        )
        .unwrap();
        assert_eq!(new_freeze, prev_freeze);
    }

    #[test]
    fn removed_feature_from_manifest() {
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: '1'

dependencies:
  b:
    version: '2'
    features:
    - xd:
        package_features: [foo]

features:
  foo: []
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
        let fetcher = Fetcher::new(&ctx).unwrap();
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let registry_origin = FullOrigin::for_registry("http://localhost:9001".to_url().unwrap());
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let manifests = HashMap::from([
            (pkg_a, Box::new(manifest_a.into_manifest())),
            (pkg_b, Box::new(manifest_b.into_manifest())),
        ]);
        let prev_a_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::from([(StrId::new("b"), pkg_b)]),
            features: HashSet::from([FeatureName::new("foo")]),
        };
        let prev_b_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::new(),
            features: HashSet::from([FeatureName::new("xd")]),
        };
        let prev_freeze = SolverFreeze {
            package_freezes: HashMap::from([(pkg_a, prev_a_freeze), (pkg_b, prev_b_freeze)]),
            main_pkg: pkg_a,
        };
        let (new_freeze, _) = block_on(
            prev_freeze
                .clone()
                .find_maximal_correct_dep_solution(&manifests, &fetcher),
        )
        .unwrap();
        let freeze_a = new_freeze.package_freezes.get(&pkg_a).unwrap();
        assert_eq!(new_freeze.package_freezes.len(), 1);
        assert!(freeze_a.dependencies_realization.is_empty());
        assert_eq!(freeze_a.features, HashSet::from([FeatureName::new("foo")]));
    }

    #[test]
    fn second_dependency_in_chain_unsatisfied() {
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
    version: '4'
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
        let fetcher = Fetcher::new(&ctx).unwrap();
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let manifest_c = parse_manifest(&path_c, &ctx).unwrap();
        let registry_origin = FullOrigin::for_registry("http://localhost:9001".to_url().unwrap());
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let identity_c = FullIdentity::new("c".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let pkg_c = PackageId::new(identity_c, Version::new(3, 0, 0));
        let manifests = HashMap::from([
            (pkg_a, Box::new(manifest_a.into_manifest())),
            (pkg_b, Box::new(manifest_b.into_manifest())),
            (pkg_c, Box::new(manifest_c.into_manifest())),
        ]);
        let prev_a_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::from([(StrId::new("b"), pkg_b)]),
            features: HashSet::new(),
        };
        let prev_b_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::from([(StrId::new("c"), pkg_c)]),
            features: HashSet::new(),
        };
        let prev_c_freeze = SolverPackageFreeze::default();
        let prev_freeze = SolverFreeze {
            package_freezes: HashMap::from([
                (pkg_a, prev_a_freeze),
                (pkg_b, prev_b_freeze),
                (pkg_c, prev_c_freeze),
            ]),
            main_pkg: pkg_a,
        };
        let (new_freeze, _) =
            block_on(prev_freeze.find_maximal_correct_dep_solution(&manifests, &fetcher)).unwrap();
        let freeze_a = new_freeze.package_freezes.get(&pkg_a).unwrap();
        let freeze_c = new_freeze.package_freezes.get(&pkg_c).unwrap();
        assert_eq!(new_freeze.package_freezes.len(), 2);
        assert!(freeze_a.dependencies_realization.is_empty());
        assert!(freeze_c.dependencies_realization.is_empty());
    }

    #[test]
    fn third_dependency_in_chain_unsatisfied() {
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
        let (_dir_b, path_c) = prepare_manifest(
            r#"
metadata:
  name: c
  version: '2'

dependencies:
  d:
    version: '5'
"#,
        );
        let (_dir_d, path_d) = prepare_manifest(
            r#"
metadata:
  name: d
  version: '4'
"#,
        );
        let ctx = DuckContext::default();
        let fetcher = Fetcher::new(&ctx).unwrap();
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let manifest_c = parse_manifest(&path_c, &ctx).unwrap();
        let manifest_d = parse_manifest(&path_d, &ctx).unwrap();
        let registry_origin = FullOrigin::for_registry("http://localhost:9001".to_url().unwrap());
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let identity_c = FullIdentity::new("c".into(), registry_origin);
        let identity_d = FullIdentity::new("d".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let pkg_c = PackageId::new(identity_c, Version::new(3, 0, 0));
        let pkg_d = PackageId::new(identity_d, Version::new(4, 0, 0));
        let manifests = HashMap::from([
            (pkg_a, Box::new(manifest_a.into_manifest())),
            (pkg_b, Box::new(manifest_b.into_manifest())),
            (pkg_c, Box::new(manifest_c.into_manifest())),
            (pkg_d, Box::new(manifest_d.into_manifest())),
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
            dependencies_realization: HashMap::from([(StrId::new("c"), pkg_c)]),
            features: HashSet::new(),
        };
        let prev_d_freeze = SolverPackageFreeze::default();
        let prev_freeze = SolverFreeze {
            package_freezes: HashMap::from([
                (pkg_a, prev_a_freeze),
                (pkg_b, prev_b_freeze),
                (pkg_c, prev_c_freeze),
                (pkg_d, prev_d_freeze),
            ]),
            main_pkg: pkg_a,
        };
        let (new_freeze, _) =
            block_on(prev_freeze.find_maximal_correct_dep_solution(&manifests, &fetcher)).unwrap();
        let freeze_a = new_freeze.package_freezes.get(&pkg_a).unwrap();
        let freeze_d = new_freeze.package_freezes.get(&pkg_d).unwrap();
        assert_eq!(new_freeze.package_freezes.len(), 2);
        assert!(freeze_a.dependencies_realization.is_empty());
        assert!(freeze_d.dependencies_realization.is_empty());
    }

    #[test]
    fn not_expansion_closed_features() {
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: '1'
"#,
        );
        let (_dir_b, path_b) = prepare_manifest(
            r#"
metadata:
  name: b
  version: '2'

features:
  expandable: [expanded]
  expanded: []
"#,
        );
        let ctx = DuckContext::default();
        let fetcher = Fetcher::new(&ctx).unwrap();
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let registry_origin = FullOrigin::for_registry("http://localhost:9001".to_url().unwrap());
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let manifests = HashMap::from([
            (pkg_a, Box::new(manifest_a.into_manifest())),
            (pkg_b, Box::new(manifest_b.into_manifest())),
        ]);
        let prev_a_freeze = SolverPackageFreeze {
            dependencies_realization: [].into(),
            features: HashSet::from([]),
        };
        let prev_b_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::new(),
            features: HashSet::from([FeatureName::new("expandable")]),
        };
        let prev_freeze = SolverFreeze {
            package_freezes: HashMap::from([
                (pkg_a, prev_a_freeze.clone()),
                (pkg_b, prev_b_freeze),
            ]),
            main_pkg: pkg_a,
        };
        let (new_freeze, _) = block_on(
            prev_freeze
                .clone()
                .find_maximal_correct_dep_solution(&manifests, &fetcher),
        )
        .unwrap();
        assert_eq!(
            new_freeze,
            SolverFreeze {
                package_freezes: [(pkg_a, prev_a_freeze)].into(),
                main_pkg: pkg_a,
            }
        )
    }
}
