use std::{
    collections::{HashMap, HashSet},
    iter::once,
};

use russcip::ProblemCreated;

use crate::{
    QuackResult, QuackResultContext, StrId,
    quackpack::core::{
        Dependency, FeatureName, Manifest, Version,
        solving::solver_model::{FoundSolution, SolverModel},
        types_common::{DependencyEdge, ExpandedLocation, ExpandedPackage, Location},
        util::get_possible_realisations,
    },
};

pub struct GatheredInfo<'a> {
    pub gathered_manifests: HashMap<ExpandedPackage, &'a Manifest>,
    pub all_possible_features: HashMap<ExpandedPackage, HashSet<FeatureName>>,
    pub versions_for_location: HashMap<ExpandedLocation, Vec<Option<Version>>>,
    pub location_resolver: HashMap<Location, ExpandedLocation>,

    pub preexisting_packages: HashSet<ExpandedPackage>,
    pub preexisting_features: HashMap<ExpandedPackage, HashSet<FeatureName>>,
}

pub fn run_engine(
    input: &GatheredInfo,
    new_dependencies: &[(ExpandedPackage, HashSet<FeatureName>)],
) -> QuackResult<FoundSolution> {
    let engine = SolverEngine::new(input);
    engine.run(new_dependencies)
}

pub struct SolverEngine<'a> {
    input: &'a GatheredInfo<'a>,
    model: SolverModel<'a, ProblemCreated>,
}

impl<'a> SolverEngine<'a> {
    fn new(input: &'a GatheredInfo) -> Self {
        Self {
            input,
            model: SolverModel::new(&input.preexisting_packages, &input.preexisting_features),
        }
    }

    fn run(
        mut self,
        new_dependencies: &[(ExpandedPackage, HashSet<FeatureName>)],
    ) -> QuackResult<FoundSolution> {
        self.create_package_variables()?;
        let empty_hashset: HashSet<FeatureName> = HashSet::new();
        for (package, manifest) in self.input.gathered_manifests.iter() {
            let possible_features = self
                .input
                .all_possible_features
                .get(package)
                .unwrap_or(&empty_hashset);
            for dependency in manifest.dependencies().all_dependencies().values() {
                if dependency.is_enabled_for(possible_features.iter().cloned()) {
                    self.construct_for_single_dependency(package, dependency)?;
                }
            }
        }

        for (new_dep, new_dep_features) in new_dependencies.iter() {
            self.model.require_package(new_dep)?;
            for feature in new_dep_features.iter() {
                self.model.require_package_with_feature(new_dep, *feature)?;
            }
        }
        self.model.solve()
    }

    fn create_package_variables(&mut self) -> QuackResult<()> {
        for pkg in self.input.gathered_manifests.keys() {
            self.model.add_package_var(pkg.clone())?;
            for possible_features in self.input.all_possible_features.get(pkg).iter() {
                for feature in possible_features.iter() {
                    self.model
                        .add_package_with_feature_var(pkg.clone(), *feature)?;
                }
            }
        }
        Ok(())
    }

    fn construct_for_single_dependency(
        &mut self,
        parent: &ExpandedPackage,
        manifest_dependency: &Dependency,
    ) -> QuackResult<()> {
        let possible_realizations = get_possible_realisations(
            manifest_dependency,
            &self.input.versions_for_location,
            &self.input.location_resolver,
        )?;

        let edge = DependencyEdge::from_manifest_and_parent(
            parent.clone(),
            manifest_dependency,
            &self.input.location_resolver,
        )
        .context_internal("Failed to expand a location")?;

        self.create_dependency_version_realization_conditions(
            &edge,
            manifest_dependency,
            &possible_realizations,
        )?;
        self.create_dependency_feature_realization_conditions(&edge, manifest_dependency)?;
        self.model.require_substantiate_dep(&edge)?;
        self.model.require_substantiate_dep_features(
            &edge,
            &self.input.all_possible_features,
            possible_realizations,
        )?;
        Ok(())
    }

    fn create_dependency_version_realization_conditions(
        &mut self,
        edge: &DependencyEdge,
        manifest_dependency: &Dependency,
        possible_realizations: &[ExpandedPackage],
    ) -> QuackResult<()> {
        for realization in possible_realizations.iter() {
            self.model
                .add_dependency_version_realisation_var(edge.clone(), realization.version)?;
        }

        let is_dep_forced_default = manifest_dependency.is_enabled_for(vec![]);
        if is_dep_forced_default {
            self.model.require_satisfying_dep_version(edge, None)?;
        } else {
            for dep_forcing_feature in manifest_dependency.enableing_features() {
                self.model
                    .require_satisfying_dep_version(edge, Some(dep_forcing_feature))?;
            }
        }
        Ok(())
    }

    fn create_dependency_feature_realization_conditions(
        &mut self,
        edge: &DependencyEdge,
        manifest_dependency: &Dependency,
    ) -> QuackResult<()> {
        let parent_features = parent_features_to_consider(self.input, edge);

        let enabled_always = HashSet::from_iter(manifest_dependency.enabled_features(vec![]));
        let mut tmp_hash_set;
        for parent_feature in parent_features {
            let forced = match parent_feature {
                None => &enabled_always,
                Some(feature) => {
                    tmp_hash_set =
                        HashSet::from_iter(manifest_dependency.enabled_features(vec![*feature]))
                            .difference(&enabled_always)
                            .cloned()
                            .collect();
                    &tmp_hash_set
                }
            };
            if forced.is_empty() {
                continue;
            }
            for feature in forced.iter() {
                self.model
                    .add_dependency_feature_realisation_var(edge.clone(), *feature)?;
            }
            self.model
                .require_satisfying_dep_feature(edge, parent_feature.cloned(), forced)?;
        }
        Ok(())
    }
}

fn parent_features_to_consider<'a>(
    input: &'a GatheredInfo,
    edge: &DependencyEdge,
) -> impl Iterator<Item = Option<&'a StrId>> {
    input
        .all_possible_features
        .get(&edge.parent)
        .into_iter()
        .flatten()
        .map(Some)
        .chain(once(None))
}

#[cfg(test)]
mod test {
    use std::path::PathBuf;

    use rustvil::fs::PathExt;
    use tempfile::{TempDir, tempdir};

    use crate::{
        DuckCtx, QpCtx,
        quackpack::core::{
            parse_manifest,
            types_common::{ExpandedLocRegistry, LocRegistry},
        },
    };

    use super::*;

    fn prepare_manifest(contents: &str) -> (TempDir, PathBuf) {
        let dir = tempdir().unwrap();
        let manifest = dir.path().join("x");
        manifest.touch().unwrap();
        manifest.write(contents).unwrap();
        (dir, manifest)
    }

    #[test]
    fn implication() {
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: 1

dependencies:
  b:
    version: 2
"#,
        );
        let (_dir_b, path_b) = prepare_manifest(
            r#"
metadata:
  name: b
  version: 2
"#,
        );
        let ctx = DuckCtx::default();
        let qpctx = QpCtx::new(&ctx);
        let manifest_a = parse_manifest(&path_a, &qpctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &qpctx).unwrap();
        let location_a = Location::Registry(LocRegistry {
            url: StrId::from("http://localhost:9001"),
            real_name: StrId::from("a"),
        });
        let location_b = Location::Registry(LocRegistry {
            url: StrId::from("http://localhost:9001"),
            real_name: StrId::from("b"),
        });
        let exp_location_a = ExpandedLocation::Registry(ExpandedLocRegistry {
            url: StrId::from("http://localhost:9001"),
            real_name: StrId::from("a"),
        });
        let exp_location_b = ExpandedLocation::Registry(ExpandedLocRegistry {
            url: StrId::from("http://localhost:9001"),
            real_name: StrId::from("b"),
        });
        let exp_pkg_a = ExpandedPackage {
            location: exp_location_a.clone(),
            version: Some(Version::new(1, 0, 0)),
        };
        let exp_pkg_b = ExpandedPackage {
            location: exp_location_b.clone(),
            version: Some(Version::new(2, 0, 0)),
        };
        let gathered_manifests = HashMap::from([
            (exp_pkg_a.clone(), manifest_a.manifest()),
            (exp_pkg_b.clone(), manifest_b.manifest()),
        ]);
        let all_possible_features = HashMap::from([
            (exp_pkg_a.clone(), HashSet::new()),
            (exp_pkg_b.clone(), HashSet::new()),
        ]);
        let versions_for_location = HashMap::from([
            (exp_location_a.clone(), vec![Some(Version::new(1, 0, 0))]),
            (exp_location_b.clone(), vec![Some(Version::new(2, 0, 0))]),
        ]);
        let location_resolver =
            HashMap::from([(location_a, exp_location_a), (location_b, exp_location_b)]);

        let preexisting_packages = HashSet::new();
        let preexisting_features = HashMap::new();

        let input = GatheredInfo {
            gathered_manifests,
            all_possible_features,
            versions_for_location,
            location_resolver,
            preexisting_packages,
            preexisting_features,
        };

        let new_dependencies = vec![(exp_pkg_a.clone(), HashSet::new())];
        let output = run_engine(&input, &new_dependencies).unwrap();
        assert!(output.new_packages == HashSet::from([exp_pkg_a, exp_pkg_b]));
    }

    #[test]
    fn equivalence_with_feature() {
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: 1

dependencies:
  b:
    version: 2

features:
  xd: []
"#,
        );
        let (_dir_b, path_b) = prepare_manifest(
            r#"
metadata:
  name: b
  version: 2

dependencies:
  a:
    version: 1
    features:
    - xd
"#,
        );
        let ctx = DuckCtx::default();
        let qpctx = QpCtx::new(&ctx);
        let manifest_a = parse_manifest(&path_a, &qpctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &qpctx).unwrap();
        let location_a = Location::Registry(LocRegistry {
            url: StrId::from("http://localhost:9001"),
            real_name: StrId::from("a"),
        });
        let location_b = Location::Registry(LocRegistry {
            url: StrId::from("http://localhost:9001"),
            real_name: StrId::from("b"),
        });
        let exp_location_a = ExpandedLocation::Registry(ExpandedLocRegistry {
            url: StrId::from("http://localhost:9001"),
            real_name: StrId::from("a"),
        });
        let exp_location_b = ExpandedLocation::Registry(ExpandedLocRegistry {
            url: StrId::from("http://localhost:9001"),
            real_name: StrId::from("b"),
        });
        let exp_pkg_a = ExpandedPackage {
            location: exp_location_a.clone(),
            version: Some(Version::new(1, 0, 0)),
        };
        let exp_pkg_b = ExpandedPackage {
            location: exp_location_b.clone(),
            version: Some(Version::new(2, 0, 0)),
        };
        let gathered_manifests = HashMap::from([
            (exp_pkg_a.clone(), manifest_a.manifest()),
            (exp_pkg_b.clone(), manifest_b.manifest()),
        ]);
        let all_possible_features = HashMap::from([
            (exp_pkg_a.clone(), HashSet::from([FeatureName::new("xd")])),
            (exp_pkg_b.clone(), HashSet::new()),
        ]);
        let versions_for_location = HashMap::from([
            (exp_location_a.clone(), vec![Some(Version::new(1, 0, 0))]),
            (exp_location_b.clone(), vec![Some(Version::new(2, 0, 0))]),
        ]);
        let location_resolver =
            HashMap::from([(location_a, exp_location_a), (location_b, exp_location_b)]);

        let preexisting_packages = HashSet::new();
        let preexisting_features = HashMap::new();

        let input = GatheredInfo {
            gathered_manifests,
            all_possible_features,
            versions_for_location,
            location_resolver,
            preexisting_packages,
            preexisting_features,
        };

        let new_dependencies = vec![(exp_pkg_a.clone(), HashSet::new())];
        let output = run_engine(&input, &new_dependencies).unwrap();
        assert!(output.new_packages == HashSet::from([exp_pkg_a.clone(), exp_pkg_b]));
        assert!(
            output.new_features
                == HashMap::from([(exp_pkg_a, HashSet::from([FeatureName::new("xd")]))])
        )
    }

    #[test]
    fn new_feature_of_preexisting_package_with_other_features() {
        // Tests a situation where the main project has only one dependency, namely `a` in version `1`.
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: 1

dependencies:
  b:
    version: 2
    features:
    - xd
"#,
        );
        let (_dir_b, path_b) = prepare_manifest(
            r#"
metadata:
  name: b
  version: 2

features:
  xd: []
  xdd: []
"#,
        );
        let ctx = DuckCtx::default();
        let qpctx = QpCtx::new(&ctx);
        let manifest_a = parse_manifest(&path_a, &qpctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &qpctx).unwrap();
        let location_a = Location::Registry(LocRegistry {
            url: StrId::from("http://localhost:9001"),
            real_name: StrId::from("a"),
        });
        let location_b = Location::Registry(LocRegistry {
            url: StrId::from("http://localhost:9001"),
            real_name: StrId::from("b"),
        });
        let exp_location_a = ExpandedLocation::Registry(ExpandedLocRegistry {
            url: StrId::from("http://localhost:9001"),
            real_name: StrId::from("a"),
        });
        let exp_location_b = ExpandedLocation::Registry(ExpandedLocRegistry {
            url: StrId::from("http://localhost:9001"),
            real_name: StrId::from("b"),
        });
        let exp_pkg_a = ExpandedPackage {
            location: exp_location_a.clone(),
            version: Some(Version::new(1, 0, 0)),
        };
        let exp_pkg_b = ExpandedPackage {
            location: exp_location_b.clone(),
            version: Some(Version::new(2, 0, 0)),
        };
        let gathered_manifests = HashMap::from([
            (exp_pkg_a.clone(), manifest_a.manifest()),
            (exp_pkg_b.clone(), manifest_b.manifest()),
        ]);
        let all_possible_features = HashMap::from([
            (exp_pkg_a.clone(), HashSet::new()),
            (
                exp_pkg_b.clone(),
                HashSet::from([FeatureName::new("xd"), FeatureName::new("xdd")]),
            ),
        ]);
        let versions_for_location = HashMap::from([
            (exp_location_a.clone(), vec![Some(Version::new(1, 0, 0))]),
            (exp_location_b.clone(), vec![Some(Version::new(2, 0, 0))]),
        ]);
        let location_resolver =
            HashMap::from([(location_a, exp_location_a), (location_b, exp_location_b)]);

        let preexisting_packages = HashSet::from([exp_pkg_b.clone()]);
        let preexisting_features =
            HashMap::from([(exp_pkg_b.clone(), HashSet::from([FeatureName::new("xdd")]))]);

        let input = GatheredInfo {
            gathered_manifests,
            all_possible_features,
            versions_for_location,
            location_resolver,
            preexisting_packages,
            preexisting_features,
        };

        let new_dependencies = vec![(exp_pkg_a.clone(), HashSet::new())];
        let output = run_engine(&input, &new_dependencies).unwrap();
        assert!(output.new_packages == HashSet::from([exp_pkg_a.clone()]));
        assert!(
            output.new_features
                == HashMap::from([(exp_pkg_b, HashSet::from([FeatureName::new("xd")]))])
        )
    }

    #[test]
    fn new_feature_of_preexisting_package_with_no_features() {
        // Tests a situation where the main project has only one dependency, namely `a` in version `1`.
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: 1

dependencies:
  b:
    version: 2
    features:
    - xd
"#,
        );
        let (_dir_b, path_b) = prepare_manifest(
            r#"
metadata:
  name: b
  version: 2

features:
  xd: []
  xdd: []
"#,
        );
        let ctx = DuckCtx::default();
        let qpctx = QpCtx::new(&ctx);
        let manifest_a = parse_manifest(&path_a, &qpctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &qpctx).unwrap();
        let location_a = Location::Registry(LocRegistry {
            url: StrId::from("http://localhost:9001"),
            real_name: StrId::from("a"),
        });
        let location_b = Location::Registry(LocRegistry {
            url: StrId::from("http://localhost:9001"),
            real_name: StrId::from("b"),
        });
        let exp_location_a = ExpandedLocation::Registry(ExpandedLocRegistry {
            url: StrId::from("http://localhost:9001"),
            real_name: StrId::from("a"),
        });
        let exp_location_b = ExpandedLocation::Registry(ExpandedLocRegistry {
            url: StrId::from("http://localhost:9001"),
            real_name: StrId::from("b"),
        });
        let exp_pkg_a = ExpandedPackage {
            location: exp_location_a.clone(),
            version: Some(Version::new(1, 0, 0)),
        };
        let exp_pkg_b = ExpandedPackage {
            location: exp_location_b.clone(),
            version: Some(Version::new(2, 0, 0)),
        };
        let gathered_manifests = HashMap::from([
            (exp_pkg_a.clone(), manifest_a.manifest()),
            (exp_pkg_b.clone(), manifest_b.manifest()),
        ]);
        let all_possible_features = HashMap::from([
            (exp_pkg_a.clone(), HashSet::new()),
            (
                exp_pkg_b.clone(),
                HashSet::from([FeatureName::new("xd"), FeatureName::new("xdd")]),
            ),
        ]);
        let versions_for_location = HashMap::from([
            (exp_location_a.clone(), vec![Some(Version::new(1, 0, 0))]),
            (exp_location_b.clone(), vec![Some(Version::new(2, 0, 0))]),
        ]);
        let location_resolver =
            HashMap::from([(location_a, exp_location_a), (location_b, exp_location_b)]);

        let preexisting_packages = HashSet::from([exp_pkg_b.clone()]);
        let preexisting_features = HashMap::new();

        let input = GatheredInfo {
            gathered_manifests,
            all_possible_features,
            versions_for_location,
            location_resolver,
            preexisting_packages,
            preexisting_features,
        };

        let new_dependencies = vec![(exp_pkg_a.clone(), HashSet::new())];
        let output = run_engine(&input, &new_dependencies).unwrap();
        assert!(output.new_packages == HashSet::from([exp_pkg_a.clone()]));
        assert!(
            output.new_features
                == HashMap::from([(exp_pkg_b, HashSet::from([FeatureName::new("xd")]))])
        )
    }
}
