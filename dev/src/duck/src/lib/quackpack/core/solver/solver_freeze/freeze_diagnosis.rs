//! Module responsible for finding a correct subfreeze inside a [`SolverFreeze`].
//! Namely, given a newly deserialized [`SolverFreeze`] this module keeps as much information from it as possible,
//! but discards things that are no longer correct.
//!
//! State of the freeze after diagnosis
//! -----------------------------------
//! What do we want:
//! 1. For any package from the freeze, the features from its freeze are present in the manifest and expansion-closed.
//! 2. For any package `pkg` from the freeze, if it has a dependency realization `(dep, realization)`, then:
//!     - `realization` is present in the freeze,
//!     - `pkg` has a dependency `dep` enabled by its features, such that `realization` realizes that dependency
//!       and enforced features are present in `realization`'s freeze.
//! 3. Any package which is not the main package has all its dependencies realized.
//! 4. Main package's freeze has the right features.
//!
//! How it is achieved
//! ------------------
//! 1. Right features are substituted into main package's freeze (pt. 4 is satisfied).
//! 2. All packages without manifest or not satisfying pt. 1 are removed (pt. 1 is satisfied).
//! 3. All disabled or no longer working realizations are removed (pt. 2 is satisfied).
//! 4. We find out which packages have all enabled dependencies realized.
//! 5. We apply DFS, recursively removing from the set above packages with realization outside of that set.
//! 6. We remove:
//!     - not-main packages outside the set constructed in steps 4-5;
//!     - realizations of the main package outside that set.
//! 
//! The last step does not break property 2, and makes property 3 satisfied.

use std::cell::RefCell;
use std::collections::{HashMap, HashSet};

use tracing::debug;

use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::solver::gathering::fetch_types::{FetchResponse, FetchSuccess};
use crate::quackpack::core::solver::gathering::gatherer::Gatherer;
use crate::quackpack::core::solver::git_access::GitAccess;
use crate::quackpack::core::solver::solver_freeze::{SolverFreeze, SolverPackageFreeze};
use crate::quackpack::core::{FeatureName, Manifest, PackageId, Selector};
use crate::util::error::ErrorsLogger;
use crate::{QuackResult, StrId, qp_bail_internal};

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

    /// Main entry point.
    /// Removes all the unnecessary or false information from the freeze, as described in the module documentation.
    /// Returns a pair consisting of the modified freeze and whether all enabled deps of main package are satisfied.
    pub async fn find_maximal_correct_dep_solution(
        mut self,
        main_features: HashSet<FeatureName>,
        manifests: &HashMap<PackageId, Box<Manifest>>,
        fetcher: &Fetcher<'_>,
    ) -> QuackResult<(Self, bool)> {
        self.substitute_root_features(main_features)?;
        self.remove_immediatelly_flawed_packages(manifests);
        self.remove_bad_realizations(manifests, fetcher).await?;
        let mut not_immediatelly_flawed = self.pkgs_with_all_deps_satisfied(manifests);
        self.retain_not_flawed_pkgs(&mut not_immediatelly_flawed)?;
        let root_satisfied = not_immediatelly_flawed.contains(&self.main_pkg);
        Ok((self, root_satisfied))
    }

    /// Helper for [`Self::find_maximal_correct_dep_solution`].
    /// Change main package features to `main_features`.
    fn substitute_root_features(&mut self, main_features: HashSet<FeatureName>) -> QuackResult<()> {
        let Some(main_freeze) = self.package_freezes.get_mut(&self.main_pkg) else {
            qp_bail_internal!("main package was not put into package freezes: {self:#?}")
        };
        main_freeze.features = main_features;
        Ok(())
    }

    /// Helper for [`Self::find_maximal_correct_dep_solution`].
    /// Removes packages for which manifest is not consistent with the freeze.
    fn remove_immediatelly_flawed_packages(
        &mut self,
        manifests: &HashMap<PackageId, Box<Manifest>>,
    ) {
        self.package_freezes
            .retain(|pkg, freeze| Self::check_manifest(*pkg, manifests, &freeze.features));
    }

    /// Helper for [`Self::remove_immediatelly_flawed_packages`].
    /// Checks if we have a manifest for the package and whether its coherent with the freeze, meaning:
    ///     * name in the manifest agrees with the name in the freeze,
    ///     * all its freeze-present features still appear in the manifest,
    ///     * freeze-present features are expansion-closed,
    ///     * freeze-present version equals manifest version.
    fn check_manifest(
        pkg: PackageId,
        manifests: &HashMap<PackageId, Box<Manifest>>,
        features: &HashSet<FeatureName>,
    ) -> bool {
        let Some(manifest) = manifests.get(&pkg) else {
            return false;
        };
        manifest.name() == pkg.name()
            && manifest.version() == pkg.version()
            && features.iter().all(|f| manifest.features().has_feature(*f))
            && manifest
                .features()
                .expand_features(features.iter().copied())
                .expect("We checked that freeze features occur in manifest")
                == *features
    }

    /// Helper for [`Self::find_maximal_correct_dep_solution`].
    /// Removes from the freeze all dependecy realizations that:
    /// - no longer realize any dependency,
    /// - the dependency is disabled with freeze-present features,
    /// - the dependency is not satisfied by the realization in the sense of [`PackageId::still_satisfies_dep`],
    /// - the dependency forces features not present in the realization's freeze.
    async fn remove_bad_realizations(
        &mut self,
        manifests: &HashMap<PackageId, Box<Manifest>>,
        fetcher: &Fetcher<'_>,
    ) -> QuackResult<()> {
        // Mapping `package` -> `dependencies_realization` keys to keep.
        // We do it this way instead of using `retain`, because of the borrow checker.
        let mut good_realizations = HashMap::new();
        for (pkg, freeze) in self.package_freezes.iter() {
            let manifest = manifests.get(pkg).expect("todo");
            let mut good_realizations_for_pkg = HashSet::new();
            for (name, realization) in freeze.dependencies_realization.iter() {
                let Some(realization_freeze) = self.package_freezes.get(realization) else {
                    continue;
                };
                if Self::still_satisfies_enabled_dep(
                    &freeze.features,
                    manifest,
                    *name,
                    *realization,
                    realization_freeze,
                    fetcher,
                )
                .await?
                {
                    good_realizations_for_pkg.insert(*name);
                }
            }
            good_realizations.insert(*pkg, good_realizations_for_pkg);
        }
        for (pkg, freeze) in self.package_freezes.iter_mut() {
            let good_realizations_for_pkg = good_realizations.remove(pkg).expect("todo");
            freeze
                .dependencies_realization
                .retain(|name, _| good_realizations_for_pkg.contains(name));
        }
        Ok(())
    }

    /// Helper for [`Self::remove_bad_realizations`].
    /// Checks that:
    /// - the dependency is satisfied by the realization in the sense of [`PackageId::still_satisfies_dep`],
    /// - all dependency-forced features are present in the realization's freeze.
    async fn still_satisfies_enabled_dep(
        pkg_features: &HashSet<FeatureName>,
        pkg_manifest: &Manifest,
        dep_name: StrId,
        realization: PackageId,
        realization_freeze: &SolverPackageFreeze,
        fetcher: &Fetcher<'_>,
    ) -> QuackResult<bool> {
        // Dependency on this name exists and is enabled.
        let Some(manifest_dependency) = pkg_manifest
            .dependencies()
            .select(&Selector::All(vec![
                Selector::EffectiveName(dep_name),
                Selector::EnabledBy(pkg_features),
            ]))
            .next()
        else {
            return Ok(false);
        };
        if !realization
            .still_satisfies_dep(manifest_dependency, fetcher)
            .await?
        {
            debug!("was satisfied before, but now is not");
            return Ok(false);
        }
        let forced_child_features = manifest_dependency.enabled_features(pkg_features);
        // Check whether features forced by the dependency on the realisation are all present in its freeze.
        // We do not have to expand them, since we have already checked in `get_manifest_and_check_features_exist`,
        // that freeze-present features are closed under expansion.
        Ok(realization_freeze
            .features
            .is_superset(&HashSet::from_iter(forced_child_features)))
    }

    /// Helper for [`Self::find_maximal_correct_dep_solution`].
    /// Finds which packages have all of their enabled dependencies correctly realized.
    /// Note:
    /// -----
    /// This should be run only after [`Self::remove_bad_realizations`].
    /// Here we assume that all realizations are correct.
    fn pkgs_with_all_deps_satisfied(
        &self,
        manifests: &HashMap<PackageId, Box<Manifest>>,
    ) -> HashSet<PackageId> {
        let mut result = HashSet::new();
        for (pkg, freeze) in self.package_freezes.iter() {
            let manifest = manifests.get(pkg).expect("todo");
            let mut all_satisfied = true;
            for dep in manifest
                .dependencies()
                .select(&Selector::EnabledBy(&freeze.features))
            {
                if !freeze
                    .dependencies_realization
                    .contains_key(&dep.effective_name())
                {
                    all_satisfied = false;
                    break;
                }
            }
            if all_satisfied {
                result.insert(*pkg);
            }
        }
        result
    }

    /// Helper for [`Self::find_maximal_correct_dep_solution`].
    /// Removes flawed non-main packages and flawed realizations of the main package.
    fn retain_not_flawed_pkgs(
        &mut self,
        still_satisfied_pkgs: &mut HashSet<PackageId>,
    ) -> QuackResult<()> {
        let reversed_graph = self.reversed_dependency_graph();
        let mut visited = HashSet::new();
        for pkg in reversed_graph.keys() {
            if !still_satisfied_pkgs.contains(pkg) {
                Self::flawed_pkgs_dfs(*pkg, &reversed_graph, &mut visited, still_satisfied_pkgs);
            }
        }
        self.package_freezes.retain(|pkg, freeze| {
            let is_root_package = *pkg == self.main_pkg;
            if is_root_package {
                freeze
                    .dependencies_realization
                    .retain(|_, realization| still_satisfied_pkgs.contains(realization));
            }
            let is_satisfied = still_satisfied_pkgs.contains(pkg);
            debug!(?pkg, %is_root_package, %is_satisfied);
            is_satisfied || is_root_package
        });
        Ok(())
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0;
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0;
        let registry_origin = FullOrigin::for_registry("http://localhost:9001".to_url().unwrap());
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let manifests = HashMap::from([
            (pkg_a, manifest_a.into_manifest()),
            (pkg_b, manifest_b.into_manifest()),
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
        let (new_freeze, _) = block_on(prev_freeze.clone().find_maximal_correct_dep_solution(
            ["foo".into()].into(),
            &manifests,
            &fetcher,
        ))
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0;
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0;
        let registry_origin = FullOrigin::for_registry("http://localhost:9001".to_url().unwrap());
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let manifests = HashMap::from([
            (pkg_a, manifest_a.into_manifest()),
            (pkg_b, manifest_b.into_manifest()),
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
        let (new_freeze, _) = block_on(prev_freeze.clone().find_maximal_correct_dep_solution(
            ["foo".into()].into(),
            &manifests,
            &fetcher,
        ))
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0;
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0;
        let manifest_c = parse_manifest(&path_c, &ctx).unwrap().0;
        let registry_origin = FullOrigin::for_registry("http://localhost:9001".to_url().unwrap());
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let identity_c = FullIdentity::new("c".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let pkg_c = PackageId::new(identity_c, Version::new(3, 0, 0));
        let manifests = HashMap::from([
            (pkg_a, manifest_a.into_manifest()),
            (pkg_b, manifest_b.into_manifest()),
            (pkg_c, manifest_c.into_manifest()),
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
        let (new_freeze, _) = block_on(prev_freeze.find_maximal_correct_dep_solution(
            [].into(),
            &manifests,
            &fetcher,
        ))
        .unwrap();
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0;
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0;
        let manifest_c = parse_manifest(&path_c, &ctx).unwrap().0;
        let manifest_d = parse_manifest(&path_d, &ctx).unwrap().0;
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
            (pkg_a, manifest_a.into_manifest()),
            (pkg_b, manifest_b.into_manifest()),
            (pkg_c, manifest_c.into_manifest()),
            (pkg_d, manifest_d.into_manifest()),
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
        let (new_freeze, _) = block_on(prev_freeze.find_maximal_correct_dep_solution(
            [].into(),
            &manifests,
            &fetcher,
        ))
        .unwrap();
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0;
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0;
        let registry_origin = FullOrigin::for_registry("http://localhost:9001".to_url().unwrap());
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let manifests = HashMap::from([
            (pkg_a, manifest_a.into_manifest()),
            (pkg_b, manifest_b.into_manifest()),
        ]);
        let prev_a_freeze = SolverPackageFreeze::default();
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
        let (new_freeze, _) = block_on(prev_freeze.clone().find_maximal_correct_dep_solution(
            [].into(),
            &manifests,
            &fetcher,
        ))
        .unwrap();
        assert_eq!(
            new_freeze,
            SolverFreeze {
                package_freezes: [(pkg_a, prev_a_freeze)].into(),
                main_pkg: pkg_a,
            }
        )
    }

    #[test]
    fn disabled_dep() {
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
    conditions:
      package-features: [foo]

features:
  foo: []
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0;
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0;
        let manifest_c = parse_manifest(&path_c, &ctx).unwrap().0;
        let registry_origin = FullOrigin::for_registry("http://localhost:9001".to_url().unwrap());
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let identity_c = FullIdentity::new("c".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let pkg_c = PackageId::new(identity_c, Version::new(3, 0, 0));
        let manifests = HashMap::from([
            (pkg_a, manifest_a.into_manifest()),
            (pkg_b, manifest_b.into_manifest()),
            (pkg_c, manifest_c.into_manifest()),
        ]);
        let prev_a_freeze = SolverPackageFreeze {
            dependencies_realization: [("b".into(), pkg_b)].into(),
            features: [].into(),
        };
        let prev_b_freeze = SolverPackageFreeze {
            dependencies_realization: [("c".into(), pkg_c)].into(),
            features: [].into(),
        };
        let prev_c_freeze = SolverPackageFreeze::default();
        let prev_freeze = SolverFreeze {
            package_freezes: HashMap::from([
                (pkg_a, prev_a_freeze.clone()),
                (pkg_b, prev_b_freeze),
                (pkg_c, prev_c_freeze),
            ]),
            main_pkg: pkg_a,
        };
        let (new_freeze, _) = block_on(prev_freeze.clone().find_maximal_correct_dep_solution(
            [].into(),
            &manifests,
            &fetcher,
        ))
        .unwrap();
        assert_eq!(
            new_freeze,
            SolverFreeze {
                package_freezes: [
                    (pkg_a, prev_a_freeze),
                    (pkg_b, SolverPackageFreeze::default()),
                    (pkg_c, SolverPackageFreeze::default())
                ]
                .into(),
                main_pkg: pkg_a,
            }
        )
    }
}
