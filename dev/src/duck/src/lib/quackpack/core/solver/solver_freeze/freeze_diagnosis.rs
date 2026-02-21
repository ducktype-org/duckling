use std::collections::{HashMap, HashSet};

use futures::future::join_all;

use crate::{
    QuackResult, QuackResultContext,
    quackpack::core::{
        Dependency, FeatureName, Manifest,
        gathering::{fetch_types::FetchResult, gatherer::Gatherer},
        git_access::GitAccess,
        solver_freeze::{SolverFreeze, SolverPackageFreeze},
        types_common::ExpandedPackage,
    },
};

impl SolverFreeze {
    /// Fetches manifests of the packages mentioned in the freeze (but not the root package),
    /// to later check whether their dependencies are still satisfied inside the freeze.
    pub async fn get_prev_freeze_manifests<'duck, GitAccessImpl: GitAccess>(
        &self,
        gatherer: &'duck Gatherer<'duck, GitAccessImpl>,
    ) -> QuackResult<HashMap<ExpandedPackage, Box<Manifest>>> {
        let mut tasks = vec![];
        for pkg in self.package_freezes.keys() {
            if *pkg == self.main_pkg {
                continue;
            }
            if let Ok(request) = pkg.create_manifest_request() {
                tasks.push(Box::pin(gatherer.fetch(request)));
            }
        }
        let results = join_all(tasks).await;
        let mut manifests = HashMap::new();
        for fetch_result in results {
            let fetch_result = fetch_result?;
            if let Some(fetch_result) = fetch_result.0 {
                match fetch_result {
                    FetchResult::Pinned(pinned_result) => {
                        manifests.insert(
                            pinned_result.expanded_package,
                            pinned_result.fetched_manifest,
                        );
                    }
                    FetchResult::NotPinned(not_pinned_result) => {
                        manifests.extend(not_pinned_result.fetched_manifests);
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
    pub fn find_maximal_correct_dep_solution(
        mut self,
        manifests: &HashMap<ExpandedPackage, Box<Manifest>>,
        new_root: ExpandedPackage,
    ) -> QuackResult<Self> {
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
        self.package_freezes
            .retain(|pkg, _| *pkg == self.main_pkg || still_satisfied_pkgs.contains(pkg));
        let main_pkg_freeze = self
            .package_freezes
            .get_mut(&self.main_pkg)
            .context_internal("Main package was not put into package freezes")?;
        main_pkg_freeze
            .dependencies_realization
            .retain(|_, realization| still_satisfied_pkgs.contains(realization));
        let main_pkg_freeze = self
            .package_freezes
            .remove(&self.main_pkg)
            .context_internal("Main package was not put into package freezes")?;
        self.package_freezes.insert(new_root, main_pkg_freeze);
        self.main_pkg = new_root;
        Ok(self)
    }

    /// Helper for [`Self::find_maximal_correct_dep_solution`].
    /// Finds which packages from the freeze are not immediatelly flawed:
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
            for (name, dep) in manifest.dependencies().all_dependencies().iter() {
                let Some(realization) = freeze.dependencies_realization.get(name) else {
                    is_every_dep_satisfied = false;
                    break;
                };
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
        if let Some(pkg_version) = pkg.version {
            if manifest.root_description().version() == pkg_version {
                Some(manifest)
            } else {
                None
            }
        } else {
            Some(manifest)
        }
    }

    /// Helper for [`Self::still_satisfied_pkgs`].
    /// Checks if a particular dependency is satisifed.
    fn check_if_dep_is_satisfied(
        freeze: &SolverPackageFreeze,
        realization_freeze: &SolverPackageFreeze,
        dep: &Dependency,
    ) -> QuackResult<bool> {
        let Some(realisation) = freeze
            .dependencies_realization
            .get(&dep.desc().manifest_name())
        else {
            // We do not have a realization of such dependency so the answer is negative.
            return Ok(false);
        };
        if !realisation.still_satisfies_dep(dep)? {
            return Ok(false);
        }
        let forced_child_features =
            dep.enabled_features(Vec::from_iter(freeze.features.iter().copied()));
        // Check whether features forced by the dependency on the realisation are all present in its freeze.
        Ok(realization_freeze
            .features
            .is_superset(&HashSet::from_iter(forced_child_features)))
    }

    /// Helper for [`Self::find_maximal_correct_dep_solution`].
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

    /// Helper for [`Self::find_maximal_correct_dep_solution`].
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
}

#[cfg(test)]
mod test {
    use std::{
        collections::{HashMap, HashSet},
        path::PathBuf,
    };

    use rustvil::fs::PathExt;
    use tempfile::{TempDir, tempdir};
    use url::Url;

    use crate::{
        DuckCtx, QpCtx, StrId,
        quackpack::core::{
            FeatureName, Version, parse_manifest,
            solver_freeze::{SolverFreeze, SolverPackageFreeze},
            types_common::{ExpandedLocation, ExpandedPackage, InternedExpandedLocation},
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
        package_features: 
        - foo

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
        let ctx = DuckCtx::default();
        let qpctx = QpCtx::new(&ctx);
        let manifest_a = parse_manifest(&path_a, &qpctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &qpctx).unwrap();
        let exp_location_a = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("a"),
        });
        let exp_location_b = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        });
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
        let new_freeze = prev_freeze
            .clone()
            .find_maximal_correct_dep_solution(&manifests, exp_pkg_a)
            .unwrap();
        assert!(new_freeze == prev_freeze);
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
        package_features: 
        - foo

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
        let ctx = DuckCtx::default();
        let qpctx = QpCtx::new(&ctx);
        let manifest_a = parse_manifest(&path_a, &qpctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &qpctx).unwrap();
        let exp_location_a = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("a"),
        });
        let exp_location_b = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        });
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
        let new_freeze = prev_freeze
            .clone()
            .find_maximal_correct_dep_solution(&manifests, exp_pkg_a)
            .unwrap();
        let freeze_a = new_freeze.package_freezes.get(&exp_pkg_a).unwrap();
        assert!(new_freeze.package_freezes.len() == 1);
        assert!(freeze_a.dependencies_realization == HashMap::new());
        assert!(freeze_a.features == HashSet::from([FeatureName::new("foo")]));
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
        let ctx = DuckCtx::default();
        let qpctx = QpCtx::new(&ctx);
        let manifest_a = parse_manifest(&path_a, &qpctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &qpctx).unwrap();
        let manifest_c = parse_manifest(&path_c, &qpctx).unwrap();
        let exp_location_a = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("a"),
        });
        let exp_location_b = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        });
        let exp_location_c = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("c"),
        });
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
        let new_freeze = prev_freeze
            .find_maximal_correct_dep_solution(&manifests, exp_pkg_a)
            .unwrap();
        let freeze_a = new_freeze.package_freezes.get(&exp_pkg_a).unwrap();
        let freeze_c = new_freeze.package_freezes.get(&exp_pkg_c).unwrap();
        assert!(new_freeze.package_freezes.len() == 2);
        assert!(freeze_a.dependencies_realization == HashMap::new());
        assert!(freeze_c.dependencies_realization == HashMap::new());
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
        let ctx = DuckCtx::default();
        let qpctx = QpCtx::new(&ctx);
        let manifest_a = parse_manifest(&path_a, &qpctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &qpctx).unwrap();
        let manifest_c = parse_manifest(&path_c, &qpctx).unwrap();
        let manifest_d = parse_manifest(&path_d, &qpctx).unwrap();
        let exp_location_a = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("a"),
        });
        let exp_location_b = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        });
        let exp_location_c = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("c"),
        });
        let exp_location_d = InternedExpandedLocation::new(ExpandedLocation::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("d"),
        });
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
        let new_freeze = prev_freeze
            .find_maximal_correct_dep_solution(&manifests, exp_pkg_a)
            .unwrap();
        let freeze_a = new_freeze.package_freezes.get(&exp_pkg_a).unwrap();
        let freeze_d = new_freeze.package_freezes.get(&exp_pkg_d).unwrap();
        assert!(new_freeze.package_freezes.len() == 2);
        assert!(freeze_a.dependencies_realization == HashMap::new());
        assert!(freeze_d.dependencies_realization == HashMap::new());
    }
}
