use std::collections::{HashMap, HashSet};

use tracing::debug;

use crate::quackpack::core::solver::gathering::fetch_types::{FetchResponse, FetchSuccess};
use crate::quackpack::core::solver::gathering::gatherer::Gatherer;
use crate::quackpack::core::solver::git_access::GitAccess;
use crate::quackpack::core::solver::solver_freeze::{SolverFreeze, SolverPackageFreeze};
use crate::quackpack::core::solver::types_common::ExpandedPackage;
use crate::quackpack::core::{Dependency, FeatureName, Manifest};
use crate::{QuackResult, QuackResultContext};

impl SolverFreeze {
    /// Fetches manifests of the packages mentioned in the freeze (but not the root package),
    /// to later check whether their dependencies are still satisfied inside the freeze.
    pub fn get_prev_freeze_manifests<Access: GitAccess>(
        &self,
        gatherer: &mut Gatherer<'_, '_, '_, Access>,
    ) -> QuackResult<HashMap<ExpandedPackage, Box<Manifest>>> {
        let mut tasks = vec![];
        for pkg in self.package_freezes.keys() {
            if *pkg == self.main_pkg {
                continue;
            }
            if let Ok(request) = pkg.create_manifest_request() {
                tasks.push(gatherer.fetch(request));
            }
        }
        let results = tasks;
        let mut manifests = HashMap::new();
        for fetch_response in results {
            let fetch_response = fetch_response?;
            if let FetchResponse::Success(success_response) = fetch_response.0 {
                match success_response {
                    FetchSuccess::Pinned(pinned_success) => {
                        manifests.insert(
                            pinned_success.expanded_package,
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
    pub fn find_maximal_correct_dep_solution(
        mut self,
        manifests: &HashMap<ExpandedPackage, Box<Manifest>>,
    ) -> QuackResult<(Self, bool)> {
        self.retain_not_flawed_pkgs(manifests)?;
        self.substitute_root_pkg(manifests)
    }

    /// Helper for [`Self::find_maximal_correct_dep_solution`]
    /// Retains freezes of the old root package and all the packages which have dependencies
    /// transitively satisfied.
    fn retain_not_flawed_pkgs(
        &mut self,
        manifests: &HashMap<ExpandedPackage, Box<Manifest>>,
    ) -> QuackResult<()> {
        let mut still_satisfied_pkgs = self.still_satisfied_pkgs(manifests)?;
        let reversed_graph = self.reversed_dependency_graph();
        let mut visited = HashSet::new();
        for pkg in reversed_graph.keys() {
            if !still_satisfied_pkgs.contains(pkg) {
                Self::flawed_pkgs_dfs(
                    pkg,
                    &reversed_graph,
                    &mut visited,
                    &mut still_satisfied_pkgs,
                );
            }
        }
        self.package_freezes.retain(|pkg, _| {
            let is_root_package = *pkg == self.main_pkg;
            let is_satisfied = still_satisfied_pkgs.contains(pkg);
            debug!(?pkg, is_root_package, is_satisfied);
            is_root_package || is_satisfied
        });
        Ok(())
    }

    /// Helper for [`Self::find_maximal_correct_dep_solution`].
    /// Finds which packages from the freeze are not immediately flawed:
    ///     * we were able to obtain their manifests,
    ///     * all manifest dependencies are satisfied by appropriate freeze-written realizations.
    fn still_satisfied_pkgs(
        &self,
        manifests: &HashMap<ExpandedPackage, Box<Manifest>>,
    ) -> QuackResult<HashSet<ExpandedPackage>> {
        let mut still_satisfied_pkgs = HashSet::new();
        for (pkg, freeze) in self.package_freezes.iter() {
            let Some(manifest) =
                Self::get_manifest_and_check_features_exist(pkg, manifests, &freeze.features)
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
                // We get rid of packages which depend on the main package, as they are not needed.
                if *realization == self.main_pkg {
                    is_every_dep_satisfied = false;
                    break;
                }
                let Some(realization_freeze) = self.package_freezes.get(realization) else {
                    is_every_dep_satisfied = false;
                    break;
                };
                is_every_dep_satisfied &=
                    Self::check_if_dep_is_satisfied(freeze, realization_freeze, dep)?;
            }
            if is_every_dep_satisfied {
                still_satisfied_pkgs.insert(*pkg);
            }
        }
        Ok(still_satisfied_pkgs)
    }

    /// Helper for [`Self::still_satisfied_pkgs`].
    /// Checks if we have a manifest for a package and whether its coherent with the freeze, meaning:
    ///     * all its freeze-present features still appear in the manifest,
    ///     * freeze-present features are expansion-closed,
    ///     * freeze-present version equals manifest version.
    fn get_manifest_and_check_features_exist<'a>(
        pkg: &ExpandedPackage,
        manifests: &'a HashMap<ExpandedPackage, Box<Manifest>>,
        features: &HashSet<FeatureName>,
    ) -> Option<&'a Manifest> {
        let manifest = manifests.get(pkg)?;
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
        if let Some(pkg_version) = pkg.version {
            if manifest.version() == pkg_version {
                Some(manifest)
            } else {
                None
            }
        } else {
            Some(manifest)
        }
    }

    /// Helper for [`Self::still_satisfied_pkgs`].
    /// Checks if a particular dependency is satisfied.
    fn check_if_dep_is_satisfied(
        freeze: &SolverPackageFreeze,
        realization_freeze: &SolverPackageFreeze,
        dep: &Dependency,
    ) -> QuackResult<bool> {
        let Some(realisation) = freeze.dependencies_realization.get(&dep.name()) else {
            // We do not have a realization of such dependency so the answer is negative.
            return Ok(false);
        };
        if !realisation.still_satisfies_dep(dep)? {
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
        cur_pkg: &ExpandedPackage,
        graph: &HashMap<ExpandedPackage, Vec<ExpandedPackage>>,
        visited: &mut HashSet<ExpandedPackage>,
        still_satisfied_pkgs: &mut HashSet<ExpandedPackage>,
    ) {
        visited.insert(*cur_pkg);
        still_satisfied_pkgs.remove(cur_pkg);
        let Some(edges) = graph.get(cur_pkg) else {
            return;
        };
        for parent in edges {
            if !visited.contains(parent) {
                Self::flawed_pkgs_dfs(parent, graph, visited, still_satisfied_pkgs);
            }
        }
    }

    /// Helper for [`Self::retain_not_flawed_pkgs`].
    /// Generates the graph used by [`Self::flawed_pkgs_dfs`].
    fn reversed_dependency_graph(&self) -> HashMap<ExpandedPackage, Vec<ExpandedPackage>> {
        let mut reversed_graph: HashMap<ExpandedPackage, Vec<ExpandedPackage>> = HashMap::new();
        for (pkg, freeze) in self.package_freezes.iter() {
            for (_, realization) in freeze.dependencies_realization.iter() {
                reversed_graph.entry(*realization).or_default().push(*pkg);
            }
        }
        reversed_graph
    }

    /// Helper for [`Self::find_maximal_correct_dep_solution`].
    /// In the freeze, removes the old root and substitutes its package freeze as the new root's package freeze.
    /// Keeps only still satisfied realizations from the old root's package freeze.
    fn substitute_root_pkg(
        mut self,
        manifests: &HashMap<ExpandedPackage, Box<Manifest>>,
    ) -> QuackResult<(Self, bool)> {
        let main_pkg_freeze = self
            .package_freezes
            .get(&self.main_pkg)
            .context_internal("Main package was not put into package freezes")?;
        let main_manifest = manifests
            .get(&self.main_pkg)
            .context_internal("Main package manifest not provided")?;
        // Check which main package dependencies are still satisfied.
        let mut still_satisfied_root_deps = HashSet::new();
        for (alias, realization) in main_pkg_freeze.dependencies_realization.iter() {
            if let Some(dependency) = main_manifest.dependencies().get_by_effective_name(*alias)
                && let Some(realization_freeze) = self.package_freezes.get(realization)
                && Self::check_if_dep_is_satisfied(main_pkg_freeze, realization_freeze, dependency)?
            {
                still_satisfied_root_deps.insert(*alias);
            }
        }
        // Leave only still satisfied dependencies.
        let main_pkg_freeze = self
            .package_freezes
            .get_mut(&self.main_pkg)
            .context_internal("Main package was not put into package freezes")?;
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
        let main_pkg_freeze = self
            .package_freezes
            .remove(&self.main_pkg)
            .context_internal("Main package was not put into package freezes")?;
        self.package_freezes.insert(self.main_pkg, main_pkg_freeze);
        Ok((self, all_main_pkg_deps_satisfied))
    }
}

#[cfg(test)]
mod test {
    use std::collections::{HashMap, HashSet};
    use std::path::PathBuf;

    use tempfile::{TempDir, tempdir};

    use crate::quackpack::core::solver::solver_freeze::{SolverFreeze, SolverPackageFreeze};
    use crate::quackpack::core::solver::types_common::{ExpandedLocation, ExpandedPackage};
    use crate::quackpack::core::{FeatureName, Version, parse_manifest};
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let exp_location_a = ExpandedLocation::Registry {
            url: "http://localhost:9001".to_url().unwrap().into(),
            real_name: StrId::from("a"),
        };
        let exp_location_b = ExpandedLocation::Registry {
            url: "http://localhost:9001".to_url().unwrap().into(),
            real_name: StrId::from("b"),
        };
        let exp_pkg_a = ExpandedPackage {
            location: exp_location_a,
            version: Some(Version::new(1, 0, 0)),
        };
        let exp_pkg_b = ExpandedPackage {
            location: exp_location_b,
            version: Some(Version::new(2, 0, 0)),
        };
        let manifests = HashMap::from([
            (exp_pkg_a, Box::new(manifest_a.manifest().clone())),
            (exp_pkg_b, Box::new(manifest_b.manifest().clone())),
        ]);
        let prev_a_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::from([(StrId::new("b"), exp_pkg_b)]),
            features: HashSet::from([FeatureName::new("foo")]),
        };
        let prev_b_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::new(),
            features: HashSet::from([FeatureName::new("xd")]),
        };
        let prev_freeze = SolverFreeze {
            package_freezes: HashMap::from([
                (exp_pkg_a, prev_a_freeze),
                (exp_pkg_b, prev_b_freeze),
            ]),
            main_pkg: exp_pkg_a,
        };
        let (new_freeze, _) = prev_freeze
            .clone()
            .find_maximal_correct_dep_solution(&manifests)
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let exp_location_a = ExpandedLocation::Registry {
            url: "http://localhost:9001".to_url().unwrap().into(),
            real_name: StrId::from("a"),
        };
        let exp_location_b = ExpandedLocation::Registry {
            url: "http://localhost:9001".to_url().unwrap().into(),
            real_name: StrId::from("b"),
        };
        let exp_pkg_a = ExpandedPackage {
            location: exp_location_a,
            version: Some(Version::new(1, 0, 0)),
        };
        let exp_pkg_b = ExpandedPackage {
            location: exp_location_b,
            version: Some(Version::new(2, 0, 0)),
        };
        let manifests = HashMap::from([
            (exp_pkg_a, Box::new(manifest_a.manifest().clone())),
            (exp_pkg_b, Box::new(manifest_b.manifest().clone())),
        ]);
        let prev_a_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::from([(StrId::new("b"), exp_pkg_b)]),
            features: HashSet::from([FeatureName::new("foo")]),
        };
        let prev_b_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::new(),
            features: HashSet::from([FeatureName::new("xd")]),
        };
        let prev_freeze = SolverFreeze {
            package_freezes: HashMap::from([
                (exp_pkg_a, prev_a_freeze),
                (exp_pkg_b, prev_b_freeze),
            ]),
            main_pkg: exp_pkg_a,
        };
        let (new_freeze, _) = prev_freeze
            .clone()
            .find_maximal_correct_dep_solution(&manifests)
            .unwrap();
        let freeze_a = new_freeze.package_freezes.get(&exp_pkg_a).unwrap();
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let manifest_c = parse_manifest(&path_c, &ctx).unwrap();
        let exp_location_a = ExpandedLocation::Registry {
            url: "http://localhost:9001".to_url().unwrap().into(),
            real_name: StrId::from("a"),
        };
        let exp_location_b = ExpandedLocation::Registry {
            url: "http://localhost:9001".to_url().unwrap().into(),
            real_name: StrId::from("b"),
        };
        let exp_location_c = ExpandedLocation::Registry {
            url: "http://localhost:9001".to_url().unwrap().into(),
            real_name: StrId::from("c"),
        };
        let exp_pkg_a = ExpandedPackage {
            location: exp_location_a,
            version: Some(Version::new(1, 0, 0)),
        };
        let exp_pkg_b = ExpandedPackage {
            location: exp_location_b,
            version: Some(Version::new(2, 0, 0)),
        };
        let exp_pkg_c = ExpandedPackage {
            location: exp_location_c,
            version: Some(Version::new(3, 0, 0)),
        };
        let manifests = HashMap::from([
            (exp_pkg_a, Box::new(manifest_a.manifest().clone())),
            (exp_pkg_b, Box::new(manifest_b.manifest().clone())),
            (exp_pkg_c, Box::new(manifest_c.manifest().clone())),
        ]);
        let prev_a_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::from([(StrId::new("b"), exp_pkg_b)]),
            features: HashSet::new(),
        };
        let prev_b_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::from([(StrId::new("c"), exp_pkg_c)]),
            features: HashSet::new(),
        };
        let prev_c_freeze = SolverPackageFreeze::default();
        let prev_freeze = SolverFreeze {
            package_freezes: HashMap::from([
                (exp_pkg_a, prev_a_freeze),
                (exp_pkg_b, prev_b_freeze),
                (exp_pkg_c, prev_c_freeze),
            ]),
            main_pkg: exp_pkg_a,
        };
        let (new_freeze, _) = prev_freeze
            .find_maximal_correct_dep_solution(&manifests)
            .unwrap();
        let freeze_a = new_freeze.package_freezes.get(&exp_pkg_a).unwrap();
        let freeze_c = new_freeze.package_freezes.get(&exp_pkg_c).unwrap();
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let manifest_c = parse_manifest(&path_c, &ctx).unwrap();
        let manifest_d = parse_manifest(&path_d, &ctx).unwrap();
        let exp_location_a = ExpandedLocation::Registry {
            url: "http://localhost:9001".to_url().unwrap().into(),
            real_name: StrId::from("a"),
        };
        let exp_location_b = ExpandedLocation::Registry {
            url: "http://localhost:9001".to_url().unwrap().into(),
            real_name: StrId::from("b"),
        };
        let exp_location_c = ExpandedLocation::Registry {
            url: "http://localhost:9001".to_url().unwrap().into(),
            real_name: StrId::from("c"),
        };
        let exp_location_d = ExpandedLocation::Registry {
            url: "http://localhost:9001".to_url().unwrap().into(),
            real_name: StrId::from("d"),
        };
        let exp_pkg_a = ExpandedPackage {
            location: exp_location_a,
            version: Some(Version::new(1, 0, 0)),
        };
        let exp_pkg_b = ExpandedPackage {
            location: exp_location_b,
            version: Some(Version::new(2, 0, 0)),
        };
        let exp_pkg_c = ExpandedPackage {
            location: exp_location_c,
            version: Some(Version::new(3, 0, 0)),
        };
        let exp_pkg_d = ExpandedPackage {
            location: exp_location_d,
            version: Some(Version::new(4, 0, 0)),
        };
        let manifests = HashMap::from([
            (exp_pkg_a, Box::new(manifest_a.manifest().clone())),
            (exp_pkg_b, Box::new(manifest_b.manifest().clone())),
            (exp_pkg_c, Box::new(manifest_c.manifest().clone())),
            (exp_pkg_d, Box::new(manifest_d.manifest().clone())),
        ]);
        let prev_a_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::from([(StrId::new("b"), exp_pkg_b)]),
            features: HashSet::new(),
        };
        let prev_b_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::from([(StrId::new("c"), exp_pkg_c)]),
            features: HashSet::new(),
        };
        let prev_c_freeze = SolverPackageFreeze {
            dependencies_realization: HashMap::from([(StrId::new("c"), exp_pkg_c)]),
            features: HashSet::new(),
        };
        let prev_d_freeze = SolverPackageFreeze::default();
        let prev_freeze = SolverFreeze {
            package_freezes: HashMap::from([
                (exp_pkg_a, prev_a_freeze),
                (exp_pkg_b, prev_b_freeze),
                (exp_pkg_c, prev_c_freeze),
                (exp_pkg_d, prev_d_freeze),
            ]),
            main_pkg: exp_pkg_a,
        };
        let (new_freeze, _) = prev_freeze
            .find_maximal_correct_dep_solution(&manifests)
            .unwrap();
        let freeze_a = new_freeze.package_freezes.get(&exp_pkg_a).unwrap();
        let freeze_d = new_freeze.package_freezes.get(&exp_pkg_d).unwrap();
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let exp_location_a = ExpandedLocation::Registry {
            url: "http://localhost:9001".to_url().unwrap().into(),
            real_name: StrId::from("a"),
        };
        let exp_location_b = ExpandedLocation::Registry {
            url: "http://localhost:9001".to_url().unwrap().into(),
            real_name: StrId::from("b"),
        };
        let exp_pkg_a = ExpandedPackage {
            location: exp_location_a,
            version: Some(Version::new(1, 0, 0)),
        };
        let exp_pkg_b = ExpandedPackage {
            location: exp_location_b,
            version: Some(Version::new(2, 0, 0)),
        };
        let manifests = HashMap::from([
            (exp_pkg_a, Box::new(manifest_a.manifest().clone())),
            (exp_pkg_b, Box::new(manifest_b.manifest().clone())),
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
                (exp_pkg_a, prev_a_freeze.clone()),
                (exp_pkg_b, prev_b_freeze),
            ]),
            main_pkg: exp_pkg_a,
        };
        let (new_freeze, _) = prev_freeze
            .clone()
            .find_maximal_correct_dep_solution(&manifests)
            .unwrap();
        assert_eq!(
            new_freeze,
            SolverFreeze {
                package_freezes: [(exp_pkg_a, prev_a_freeze)].into(),
                main_pkg: exp_pkg_a,
            }
        )
    }
}
