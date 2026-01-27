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
        types_common::{
            DependencyEdge, ExpandedPackage, InternedExpandedLocation, InternedLocation,
        },
        util::get_possible_realisations,
    },
};

// Input for solver engine type, currently here, after implementing the gathering information stage will be moved there.
#[derive(Debug)]
pub struct GatheredInfo<'a> {
    pub gathered_manifests: HashMap<ExpandedPackage, &'a Manifest>,
    pub all_possible_features: HashMap<ExpandedPackage, HashSet<FeatureName>>,
    pub versions_for_location: HashMap<InternedExpandedLocation, Vec<Option<Version>>>,
    pub location_resolver: HashMap<InternedLocation, InternedExpandedLocation>,

    pub preexisting_packages: HashSet<ExpandedPackage>,
    pub preexisting_features: HashMap<ExpandedPackage, HashSet<FeatureName>>,
    pub preexisting_dependencies: HashMap<DependencyEdge, Option<Version>>,
}

#[derive(Debug)]
/// Struct performing dependencies resolving.
pub struct SolverEngine<'a> {
    input: &'a GatheredInfo<'a>,
    model: SolverModel<'a, ProblemCreated>,
}

impl<'a> SolverEngine<'a> {
    /// Main entry point.
    /// Creates an engine and runs it.
    pub fn run_engine(
        input: &GatheredInfo,
        new_dependencies: &[(ExpandedPackage, HashSet<FeatureName>)],
    ) -> QuackResult<FoundSolution> {
        let engine = SolverEngine::new(input);
        engine.run(new_dependencies)
    }
    /// Creates a new SolverEngine from the given GatheredInfo reference.
    fn new(input: &'a GatheredInfo) -> Self {
        Self {
            input,
            model: SolverModel::new(&input.preexisting_packages, &input.preexisting_features),
        }
    }

    /// Runs the engine, building the underlying solver model, solving it and returning the output.
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
                self.model.require_package_with_feature(new_dep, feature)?;
            }
        }
        self.model.solve()
    }

    /// Creates necesseary varaiables for all the packages.
    fn create_package_variables(&mut self) -> QuackResult<()> {
        for pkg in self.input.gathered_manifests.keys() {
            self.model.add_package_var(*pkg);
            for feature in self
                .input
                .all_possible_features
                .get(pkg)
                .iter()
                .copied()
                .flatten()
            {
                self.model.add_package_with_feature_var(*pkg, *feature);
            }
        }
        Ok(())
    }

    /// Creates the necesseary constraints for a single dependency.
    fn construct_for_single_dependency(
        &mut self,
        parent: &ExpandedPackage,
        manifest_dependency: &Dependency,
    ) -> QuackResult<()> {
        let edge = DependencyEdge::from_manifest_and_parent(
            *parent,
            manifest_dependency,
            &self.input.location_resolver,
        )
        .context_internal("Failed to expand a location")?;

        // If this edge was not resolved in the previous freeze, we fallback to adding all constraints.
        let Some(realization_ver) = self.input.preexisting_dependencies.get(&edge) else {
            return self.add_constraints_for_edge(&edge, manifest_dependency);
        };
        let realisation = ExpandedPackage {
            location: edge.dependency_loc,
            version: *realization_ver,
        };

        // Each feature of the parent may force some additional features of the child,
        // not present in the previous freeze.
        // We try to add them to the chosen realisation.
        let mut forcing = vec![];
        let possible_child_features = self
            .input
            .all_possible_features
            .get(&realisation)
            .context_internal("Possible features map does not contain looked up package")?;
        for parent_feature in self
            .input
            .all_possible_features
            .get(parent)
            .iter()
            .cloned()
            .flatten()
        {
            if let Some(parent_preexisting) = self.input.preexisting_features.get(parent)
                && parent_preexisting.contains(parent_feature)
            {
                // Parent feature belonged to the previous freeze, so whatever it forced, has been already taken care of.
                continue;
            }
            let forced = manifest_dependency.enabled_features(vec![*parent_feature]);
            if forced
                .iter()
                .any(|feature| !possible_child_features.contains(feature))
            {
                // (*) Previously chosen realisation of the dependency does not support some of the forced flags,
                // so we have to treat the dependency normally and add all the constraints.
                return self.add_constraints_for_edge(&edge, manifest_dependency);
            } else {
                forcing.push((*parent_feature, forced));
            }
        }
        // If (*) never happened, we just add conditions that parent feature forces some new realisation features.
        self.model
            .require_satisfying_dep_feature_for_preexisting(parent, &realisation, forcing)
    }

    /// Creates all standard constraints for a dependency edge.
    fn add_constraints_for_edge(
        &mut self,
        edge: &DependencyEdge,
        manifest_dependency: &Dependency,
    ) -> QuackResult<()> {
        let possible_realizations = get_possible_realisations(
            manifest_dependency,
            &self.input.versions_for_location,
            &self.input.location_resolver,
        )?;

        self.create_dependency_version_realization_conditions(
            edge,
            manifest_dependency,
            &possible_realizations,
        )?;
        self.create_dependency_feature_realization_conditions(edge, manifest_dependency)?;
        self.model.require_substantiate_dep(edge)?;
        self.model.require_substantiate_dep_features(
            edge,
            &self.input.all_possible_features,
            &possible_realizations,
        )?;
        Ok(())
    }

    /// Creates version realization constraints and necesseary variables for a single dependency.
    fn create_dependency_version_realization_conditions(
        &mut self,
        edge: &DependencyEdge,
        manifest_dependency: &Dependency,
        possible_realizations: &[ExpandedPackage],
    ) -> QuackResult<()> {
        for realization in possible_realizations {
            self.model
                .add_dependency_version_realisation_var(edge.clone(), realization.version);
        }

        let is_dep_forced_default = manifest_dependency.is_enabled_for(vec![]);
        if is_dep_forced_default {
            self.model.require_satisfying_dep_version(edge, None)?;
        } else {
            for dep_forcing_feature in manifest_dependency.enableing_features()? {
                self.model
                    .require_satisfying_dep_version(edge, Some(dep_forcing_feature))?;
            }
        }
        Ok(())
    }

    /// Creates feature realization constraints and necesseary variables for a single dependency.
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
                    .add_dependency_feature_realisation_var(edge.clone(), *feature);
            }
            self.model
                .require_satisfying_dep_feature(edge, parent_feature, forced)?;
        }
        Ok(())
    }
}

/// Creates an iterator of all possible parent features and None.
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
    use url::Url;

    use crate::{
        DuckCtx, QpCtx,
        quackpack::core::{
            parse_manifest,
            types_common::{ExpandedLocRegistry, ExpandedLocation, LocRegistry, Location},
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
    /// Tests simple implication of form `a` requires `b`, with `a` required.
    fn implication() {
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
"#,
        );
        let ctx = DuckCtx::default();
        let qpctx = QpCtx::new(&ctx);
        let manifest_a = parse_manifest(&path_a, &qpctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &qpctx).unwrap();
        let location_a = InternedLocation::new(Location::Registry(LocRegistry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("a"),
        }));
        let location_b = InternedLocation::new(Location::Registry(LocRegistry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        }));
        let exp_location_a =
            InternedExpandedLocation::new(ExpandedLocation::Registry(ExpandedLocRegistry {
                url: Url::parse("http://localhost:9001").unwrap(),
                real_name: StrId::from("a"),
            }));
        let exp_location_b =
            InternedExpandedLocation::new(ExpandedLocation::Registry(ExpandedLocRegistry {
                url: Url::parse("http://localhost:9001").unwrap(),
                real_name: StrId::from("b"),
            }));
        let exp_pkg_a = ExpandedPackage {
            location: exp_location_a,
            version: Some(Version::new(1, 0, 0)),
        };
        let exp_pkg_b = ExpandedPackage {
            location: exp_location_b,
            version: Some(Version::new(2, 0, 0)),
        };
        let gathered_manifests = HashMap::from([
            (exp_pkg_a, manifest_a.manifest()),
            (exp_pkg_b, manifest_b.manifest()),
        ]);
        let all_possible_features =
            HashMap::from([(exp_pkg_a, HashSet::new()), (exp_pkg_b, HashSet::new())]);
        let versions_for_location = HashMap::from([
            (exp_location_a, vec![Some(Version::new(1, 0, 0))]),
            (exp_location_b, vec![Some(Version::new(2, 0, 0))]),
        ]);
        let location_resolver = HashMap::from([
            (location_a, exp_location_a),
            (location_b, exp_location_b.clone()),
        ]);

        let input = GatheredInfo {
            gathered_manifests,
            all_possible_features,
            versions_for_location,
            location_resolver,
            preexisting_packages: HashSet::new(),
            preexisting_features: HashMap::new(),
            preexisting_dependencies: HashMap::new(),
        };

        let new_dependencies = vec![(exp_pkg_a.clone(), HashSet::new())];
        let output = SolverEngine::run_engine(&input, &new_dependencies).unwrap();
        assert!(output.new_packages == HashSet::from([exp_pkg_a.clone(), exp_pkg_b.clone()]));
        assert!(output.new_features == HashMap::new());
        assert!(
            output.new_edges
                == HashMap::from([(
                    DependencyEdge {
                        parent: exp_pkg_a,
                        dependency_loc: exp_location_b,
                    },
                    Some(Version::new(2, 0, 0))
                )])
        );
    }

    #[test]
    /// Tests a situation where `a` requires `b` and `b` requires `a` with `xd`, with `a` required.
    fn equivalence_with_feature() {
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: '1'

dependencies:
  b:
    version: '2'

features:
  xd: []
"#,
        );
        let (_dir_b, path_b) = prepare_manifest(
            r#"
metadata:
  name: b
  version: '2'

dependencies:
  a:
    version: '1'
    features:
    - xd
"#,
        );
        let ctx = DuckCtx::default();
        let qpctx = QpCtx::new(&ctx);
        let manifest_a = parse_manifest(&path_a, &qpctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &qpctx).unwrap();
        let location_a = InternedLocation::new(Location::Registry(LocRegistry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("a"),
        }));
        let location_b = InternedLocation::new(Location::Registry(LocRegistry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        }));
        let exp_location_a =
            InternedExpandedLocation::new(ExpandedLocation::Registry(ExpandedLocRegistry {
                url: Url::parse("http://localhost:9001").unwrap(),
                real_name: StrId::from("a"),
            }));
        let exp_location_b =
            InternedExpandedLocation::new(ExpandedLocation::Registry(ExpandedLocRegistry {
                url: Url::parse("http://localhost:9001").unwrap(),
                real_name: StrId::from("b"),
            }));
        let exp_pkg_a = ExpandedPackage {
            location: exp_location_a,
            version: Some(Version::new(1, 0, 0)),
        };
        let exp_pkg_b = ExpandedPackage {
            location: exp_location_b,
            version: Some(Version::new(2, 0, 0)),
        };
        let gathered_manifests = HashMap::from([
            (exp_pkg_a, manifest_a.manifest()),
            (exp_pkg_b, manifest_b.manifest()),
        ]);
        let all_possible_features = HashMap::from([
            (exp_pkg_a, HashSet::from([FeatureName::new("xd")])),
            (exp_pkg_b, HashSet::new()),
        ]);
        let versions_for_location = HashMap::from([
            (exp_location_a, vec![Some(Version::new(1, 0, 0))]),
            (exp_location_b, vec![Some(Version::new(2, 0, 0))]),
        ]);
        let location_resolver =
            HashMap::from([(location_a, exp_location_a), (location_b, exp_location_b)]);

        let input = GatheredInfo {
            gathered_manifests,
            all_possible_features,
            versions_for_location,
            location_resolver,
            preexisting_packages: HashSet::new(),
            preexisting_features: HashMap::new(),
            preexisting_dependencies: HashMap::new(),
        };

        let new_dependencies = vec![(exp_pkg_a, HashSet::new())];
        let output = SolverEngine::run_engine(&input, &new_dependencies).unwrap();
        assert!(output.new_packages == HashSet::from([exp_pkg_a, exp_pkg_b]));
        assert!(
            output.new_features
                == HashMap::from([(exp_pkg_a, HashSet::from([FeatureName::new("xd")]))])
        );
        assert!(
            output.new_edges
                == HashMap::from([
                    (
                        DependencyEdge {
                            parent: exp_pkg_a,
                            dependency_loc: exp_location_b,
                        },
                        Some(Version::new(2, 0, 0))
                    ),
                    (
                        DependencyEdge {
                            parent: exp_pkg_b,
                            dependency_loc: exp_location_a,
                        },
                        Some(Version::new(1, 0, 0))
                    ),
                ])
        );
    }

    #[test]
    /// `a` is required, `a` requires `b` with `xd`, `b` preexists with `xdd`.
    fn new_feature_of_preexisting_package_with_other_features() {
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: '1'

dependencies:
  b:
    version: '2'
    features:
    - xd
"#,
        );
        let (_dir_b, path_b) = prepare_manifest(
            r#"
metadata:
  name: b
  version: '2'

features:
  xd: []
  xdd: []
"#,
        );
        let ctx = DuckCtx::default();
        let qpctx = QpCtx::new(&ctx);
        let manifest_a = parse_manifest(&path_a, &qpctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &qpctx).unwrap();
        let location_a = InternedLocation::new(Location::Registry(LocRegistry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("a"),
        }));
        let location_b = InternedLocation::new(Location::Registry(LocRegistry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        }));
        let exp_location_a =
            InternedExpandedLocation::new(ExpandedLocation::Registry(ExpandedLocRegistry {
                url: Url::parse("http://localhost:9001").unwrap(),
                real_name: StrId::from("a"),
            }));
        let exp_location_b =
            InternedExpandedLocation::new(ExpandedLocation::Registry(ExpandedLocRegistry {
                url: Url::parse("http://localhost:9001").unwrap(),
                real_name: StrId::from("b"),
            }));
        let exp_pkg_a = ExpandedPackage {
            location: exp_location_a,
            version: Some(Version::new(1, 0, 0)),
        };
        let exp_pkg_b = ExpandedPackage {
            location: exp_location_b,
            version: Some(Version::new(2, 0, 0)),
        };
        let gathered_manifests = HashMap::from([
            (exp_pkg_a, manifest_a.manifest()),
            (exp_pkg_b, manifest_b.manifest()),
        ]);
        let all_possible_features = HashMap::from([
            (exp_pkg_a, HashSet::new()),
            (
                exp_pkg_b,
                HashSet::from([FeatureName::new("xd"), FeatureName::new("xdd")]),
            ),
        ]);
        let versions_for_location = HashMap::from([
            (exp_location_a, vec![Some(Version::new(1, 0, 0))]),
            (exp_location_b, vec![Some(Version::new(2, 0, 0))]),
        ]);
        let location_resolver =
            HashMap::from([(location_a, exp_location_a), (location_b, exp_location_b)]);

        let preexisting_packages = HashSet::from([exp_pkg_b]);
        let preexisting_features =
            HashMap::from([(exp_pkg_b, HashSet::from([FeatureName::new("xdd")]))]);

        let input = GatheredInfo {
            gathered_manifests,
            all_possible_features,
            versions_for_location,
            location_resolver,
            preexisting_packages,
            preexisting_features,
            preexisting_dependencies: HashMap::new(),
        };

        let new_dependencies = vec![(exp_pkg_a, HashSet::new())];
        let output = SolverEngine::run_engine(&input, &new_dependencies).unwrap();
        assert!(output.new_packages == HashSet::from([exp_pkg_a]));
        assert!(
            output.new_features
                == HashMap::from([(exp_pkg_b, HashSet::from([FeatureName::new("xd")]))])
        )
    }

    #[test]
    /// `a` is required, `a` requires `b` with `xd`, `b` preexists with no features.
    fn new_feature_of_preexisting_package_with_no_features() {
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: '1'

dependencies:
  b:
    version: '2'
    features:
    - xd
"#,
        );
        let (_dir_b, path_b) = prepare_manifest(
            r#"
metadata:
  name: b
  version: '2'

features:
  xd: []
  xdd: []
"#,
        );
        let ctx = DuckCtx::default();
        let qpctx = QpCtx::new(&ctx);
        let manifest_a = parse_manifest(&path_a, &qpctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &qpctx).unwrap();
        let location_a = InternedLocation::new(Location::Registry(LocRegistry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("a"),
        }));
        let location_b = InternedLocation::new(Location::Registry(LocRegistry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        }));
        let exp_location_a =
            InternedExpandedLocation::new(ExpandedLocation::Registry(ExpandedLocRegistry {
                url: Url::parse("http://localhost:9001").unwrap(),
                real_name: StrId::from("a"),
            }));
        let exp_location_b =
            InternedExpandedLocation::new(ExpandedLocation::Registry(ExpandedLocRegistry {
                url: Url::parse("http://localhost:9001").unwrap(),
                real_name: StrId::from("b"),
            }));
        let exp_pkg_a = ExpandedPackage {
            location: exp_location_a,
            version: Some(Version::new(1, 0, 0)),
        };
        let exp_pkg_b = ExpandedPackage {
            location: exp_location_b,
            version: Some(Version::new(2, 0, 0)),
        };
        let gathered_manifests = HashMap::from([
            (exp_pkg_a, manifest_a.manifest()),
            (exp_pkg_b, manifest_b.manifest()),
        ]);
        let all_possible_features = HashMap::from([
            (exp_pkg_a, HashSet::new()),
            (
                exp_pkg_b,
                HashSet::from([FeatureName::new("xd"), FeatureName::new("xdd")]),
            ),
        ]);
        let versions_for_location = HashMap::from([
            (exp_location_a, vec![Some(Version::new(1, 0, 0))]),
            (exp_location_b, vec![Some(Version::new(2, 0, 0))]),
        ]);
        let location_resolver =
            HashMap::from([(location_a, exp_location_a), (location_b, exp_location_b)]);

        let preexisting_packages = HashSet::from([exp_pkg_b]);

        let input = GatheredInfo {
            gathered_manifests,
            all_possible_features,
            versions_for_location,
            location_resolver,
            preexisting_packages,
            preexisting_features: HashMap::new(),
            preexisting_dependencies: HashMap::new(),
        };

        let new_dependencies = vec![(exp_pkg_a.clone(), HashSet::new())];
        let output = SolverEngine::run_engine(&input, &new_dependencies).unwrap();
        assert!(output.new_packages == HashSet::from([exp_pkg_a.clone()]));
        assert!(
            output.new_features
                == HashMap::from([(exp_pkg_b, HashSet::from([FeatureName::new("xd")]))])
        )
    }

    #[test]
    /// `a` is required, `a` requires `b` with `xd`, `b` requires `c`, with `xdd` if `b` with `xd`;
    /// `b` preexists with no features, as well as `c`.
    fn new_feature_of_preexisting_package_chain_with_no_features() {
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: '1'

dependencies:
  b:
    version: '2'
    features:
    - xd
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
    features:
    - xdd:
        package_features: [xd]

features:
  xd: []
"#,
        );
        let (_dir_c, path_c) = prepare_manifest(
            r#"
metadata:
  name: c
  version: '3'

features:
  xdd: []
"#,
        );
        let ctx = DuckCtx::default();
        let qpctx = QpCtx::new(&ctx);
        let manifest_a = parse_manifest(&path_a, &qpctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &qpctx).unwrap();
        let manifest_c = parse_manifest(&path_c, &qpctx).unwrap();
        let location_a = InternedLocation::new(Location::Registry(LocRegistry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("a"),
        }));
        let location_b = InternedLocation::new(Location::Registry(LocRegistry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        }));
        let location_c = InternedLocation::new(Location::Registry(LocRegistry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("c"),
        }));
        let exp_location_a =
            InternedExpandedLocation::new(ExpandedLocation::Registry(ExpandedLocRegistry {
                url: Url::parse("http://localhost:9001").unwrap(),
                real_name: StrId::from("a"),
            }));
        let exp_location_b =
            InternedExpandedLocation::new(ExpandedLocation::Registry(ExpandedLocRegistry {
                url: Url::parse("http://localhost:9001").unwrap(),
                real_name: StrId::from("b"),
            }));
        let exp_location_c =
            InternedExpandedLocation::new(ExpandedLocation::Registry(ExpandedLocRegistry {
                url: Url::parse("http://localhost:9001").unwrap(),
                real_name: StrId::from("c"),
            }));
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
        let gathered_manifests = HashMap::from([
            (exp_pkg_a, manifest_a.manifest()),
            (exp_pkg_b, manifest_b.manifest()),
            (exp_pkg_c, manifest_c.manifest()),
        ]);
        let all_possible_features = HashMap::from([
            (exp_pkg_a, HashSet::new()),
            (exp_pkg_b, HashSet::from([FeatureName::new("xd")])),
            (exp_pkg_c, HashSet::from([FeatureName::new("xdd")])),
        ]);
        let versions_for_location = HashMap::from([
            (exp_location_a, vec![Some(Version::new(1, 0, 0))]),
            (exp_location_b, vec![Some(Version::new(2, 0, 0))]),
            (exp_location_c, vec![Some(Version::new(3, 0, 0))]),
        ]);
        let location_resolver = HashMap::from([
            (location_a, exp_location_a),
            (location_b, exp_location_b),
            (location_c, exp_location_c),
        ]);

        let preexisting_packages = HashSet::from([exp_pkg_b, exp_pkg_c]);
        let preexisting_dependencies = HashMap::from([(
            DependencyEdge {
                parent: exp_pkg_b,
                dependency_loc: exp_location_c,
            },
            Some(Version::new(3, 0, 0)),
        )]);

        let input = GatheredInfo {
            gathered_manifests,
            all_possible_features,
            versions_for_location,
            location_resolver,
            preexisting_packages,
            preexisting_features: HashMap::new(),
            preexisting_dependencies,
        };

        let new_dependencies = vec![(exp_pkg_a, HashSet::new())];
        let output = SolverEngine::run_engine(&input, &new_dependencies).unwrap();
        assert!(output.new_packages == HashSet::from([exp_pkg_a]));
        assert!(
            output.new_features
                == HashMap::from([
                    (exp_pkg_b, HashSet::from([FeatureName::new("xd")])),
                    (exp_pkg_c, HashSet::from([FeatureName::new("xdd")]))
                ])
        )
    }
}
