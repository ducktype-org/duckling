use std::{
    collections::{HashMap, HashSet},
    iter::once,
};

use russcip::ProblemCreated;

use crate::{
    QuackResult, QuackResultContext, StrId,
    quackpack::core::{
        Dependency, FeatureName, Manifest, Version,
        gathering::gatherer_state::GatheredInfo,
        solver_freeze::SolverFreeze,
        solving::solver_model::{FoundSolution, SolverModel},
        types_common::{
            DependencyEdge, ExpandedPackage, InternedExpandedLocation, InternedLocation, Location,
        },
        util::get_possible_realizations,
    },
};

/// Struct with all the necessary information for the solver to be run.
#[derive(Debug)]
pub struct SolverInput {
    pub gathered_manifests: HashMap<ExpandedPackage, Box<Manifest>>,
    pub all_possible_features: HashMap<ExpandedPackage, HashSet<FeatureName>>,
    pub versions_for_location: HashMap<InternedExpandedLocation, HashSet<Option<Version>>>,
    pub location_resolver: HashMap<InternedLocation, InternedExpandedLocation>,

    pub preexisting_packages: HashSet<ExpandedPackage>,
    pub preexisting_features: HashMap<ExpandedPackage, HashSet<FeatureName>>,
    pub preexisting_dependencies: HashMap<DependencyEdge, Option<Version>>,
}

impl SolverInput {
    /// Creates the solver input, based on the previous freeze, its packages' manifests and information gathered
    /// in the gathering phase.
    #[tracing::instrument(skip_all)]
    pub fn from_freeze_and_gathered_info(
        prev_freeze: &SolverFreeze,
        prev_freeze_manifests: HashMap<ExpandedPackage, Box<Manifest>>,
        gathered_info: GatheredInfo,
    ) -> Self {
        let mut gathered_manifests = gathered_info.gathered_manifests;
        for (pkg, manifest) in prev_freeze_manifests {
            if prev_freeze.package_freezes.contains_key(&pkg) {
                gathered_manifests.insert(pkg, manifest);
            }
        }
        let mut all_possible_features = gathered_info.possible_features;
        let mut versions_for_location = gathered_info.versions_for_location;
        let mut location_resolver = gathered_info.location_resolver;
        let mut preexisting_packages = HashSet::new();
        let mut preexisting_features = HashMap::new();
        let mut preexisting_dependencies = HashMap::new();
        for (pkg, freeze) in prev_freeze.package_freezes.iter() {
            all_possible_features
                .entry(*pkg)
                .or_default()
                .extend(freeze.features.iter().copied());
            versions_for_location
                .entry(pkg.location)
                .or_default()
                .insert(pkg.version);
            location_resolver.insert(
                InternedLocation::new(Location::canonical_unexpansion(pkg.location())),
                pkg.location,
            );
            preexisting_packages.insert(*pkg);
            preexisting_features.insert(*pkg, freeze.features.clone());
            for (dep_name, realization) in freeze.dependencies_realization.iter() {
                preexisting_dependencies.insert(
                    DependencyEdge {
                        parent: *pkg,
                        dependency_loc: realization.location,
                        manifest_child_name: *dep_name,
                    },
                    realization.version,
                );
            }
        }
        Self {
            gathered_manifests,
            all_possible_features,
            versions_for_location,
            location_resolver,
            preexisting_packages,
            preexisting_features,
            preexisting_dependencies,
        }
    }
}

#[derive(Debug)]
/// Struct performing dependencies resolving.
pub struct SolverEngine<'a> {
    input: &'a SolverInput,
    model: SolverModel<'a, ProblemCreated>,
}

impl<'a> SolverEngine<'a> {
    /// Main entry point.
    /// Creates an engine and runs it.
    #[tracing::instrument(skip_all)]
    pub fn run_engine(
        input: SolverInput,
        main_pkg: &(ExpandedPackage, HashSet<FeatureName>),
    ) -> QuackResult<FoundSolution> {
        let engine = SolverEngine::new(&input);
        engine.run(main_pkg)
    }

    /// Creates a new [`SolverEngine`] from the given [`SolverInput`] reference.
    fn new(input: &'a SolverInput) -> Self {
        Self {
            input,
            model: SolverModel::new(&input.preexisting_packages, &input.preexisting_features),
        }
    }

    /// Runs the engine, building the underlying solver model, solving it and returning the output.
    fn run(
        mut self,
        main_pkg: &(ExpandedPackage, HashSet<FeatureName>),
    ) -> QuackResult<FoundSolution> {
        self.create_package_variables();
        let empty_hashset: HashSet<FeatureName> = HashSet::new();
        for (package, manifest) in self.input.gathered_manifests.iter() {
            let possible_features = self
                .input
                .all_possible_features
                .get(package)
                .unwrap_or(&empty_hashset);
            for dependency in manifest.dependencies().all_dependencies() {
                if dependency.is_enabled_for(possible_features.iter().cloned()) {
                    self.construct_for_single_dependency(package, dependency)?;
                }
            }
        }

        self.model.require_package(&main_pkg.0)?;
        for feature in main_pkg.1.iter() {
            self.model
                .require_package_with_feature(&main_pkg.0, feature)?;
        }
        self.model.solve()
    }

    /// Creates necessary variables for all the packages.
    fn create_package_variables(&mut self) {
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
        let possible_realizations = get_possible_realizations(
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
            for dep_forcing_feature in manifest_dependency.enableing_features() {
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
/// This function is used when trying to determine
/// which features of the child are forced by which features of the parent.
/// The [`None`] signifies the lack of any parent features,
/// so that features of the child forced by default can be considered.
fn parent_features_to_consider<'a>(
    input: &'a SolverInput,
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

    use tempfile::{TempDir, tempdir};
    use url::Url;

    use crate::{
        DuckCtx,
        quackpack::core::{
            parse_manifest,
            types_common::{ExpandedLocation, Location},
        },
        util::path_ops_ext::PathOpsExt,
    };

    use super::*;

    fn prepare_manifest(contents: &str) -> (TempDir, PathBuf) {
        let dir = tempdir().unwrap();
        let manifest = dir.path().join("x");
        manifest.touch().unwrap();
        manifest.write(contents).unwrap();
        dir.path().try_fsync_dir().unwrap();
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let location_a = InternedLocation::new(Location::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("a"),
        });
        let location_b = InternedLocation::new(Location::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        });
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
        let gathered_manifests = HashMap::from([
            (exp_pkg_a, Box::new(manifest_a.manifest().clone())),
            (exp_pkg_b, Box::new(manifest_b.manifest().clone())),
        ]);
        let all_possible_features =
            HashMap::from([(exp_pkg_a, HashSet::new()), (exp_pkg_b, HashSet::new())]);
        let versions_for_location = HashMap::from([
            (exp_location_a, HashSet::from([Some(Version::new(1, 0, 0))])),
            (exp_location_b, HashSet::from([Some(Version::new(2, 0, 0))])),
        ]);
        let location_resolver =
            HashMap::from([(location_a, exp_location_a), (location_b, exp_location_b)]);

        let input = SolverInput {
            gathered_manifests,
            all_possible_features,
            versions_for_location,
            location_resolver,
            preexisting_packages: HashSet::new(),
            preexisting_features: HashMap::new(),
            preexisting_dependencies: HashMap::new(),
        };

        let main_pkg = (exp_pkg_a, HashSet::new());
        let output = SolverEngine::run_engine(input, &main_pkg).unwrap();
        assert!(output.new_packages == HashSet::from([exp_pkg_a, exp_pkg_b]));
        assert!(output.new_features.is_empty());
        assert!(
            output.new_edges
                == HashMap::from([(
                    DependencyEdge {
                        parent: exp_pkg_a,
                        dependency_loc: exp_location_b,
                        manifest_child_name: StrId::new("b"),
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let location_a = InternedLocation::new(Location::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("a"),
        });
        let location_b = InternedLocation::new(Location::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        });
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
        let gathered_manifests = HashMap::from([
            (exp_pkg_a, Box::new(manifest_a.manifest().clone())),
            (exp_pkg_b, Box::new(manifest_b.manifest().clone())),
        ]);
        let all_possible_features = HashMap::from([
            (exp_pkg_a, HashSet::from([FeatureName::new("xd")])),
            (exp_pkg_b, HashSet::new()),
        ]);
        let versions_for_location = HashMap::from([
            (exp_location_a, HashSet::from([Some(Version::new(1, 0, 0))])),
            (exp_location_b, HashSet::from([Some(Version::new(2, 0, 0))])),
        ]);
        let location_resolver =
            HashMap::from([(location_a, exp_location_a), (location_b, exp_location_b)]);

        let input = SolverInput {
            gathered_manifests,
            all_possible_features,
            versions_for_location,
            location_resolver,
            preexisting_packages: HashSet::new(),
            preexisting_features: HashMap::new(),
            preexisting_dependencies: HashMap::new(),
        };

        let main_pkg = (exp_pkg_a, HashSet::new());
        let output = SolverEngine::run_engine(input, &main_pkg).unwrap();
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
                            manifest_child_name: StrId::new("b"),
                        },
                        Some(Version::new(2, 0, 0))
                    ),
                    (
                        DependencyEdge {
                            parent: exp_pkg_b,
                            dependency_loc: exp_location_a,
                            manifest_child_name: StrId::new("a"),
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let location_a = InternedLocation::new(Location::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("a"),
        });
        let location_b = InternedLocation::new(Location::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        });
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
        let gathered_manifests = HashMap::from([
            (exp_pkg_a, Box::new(manifest_a.manifest().clone())),
            (exp_pkg_b, Box::new(manifest_b.manifest().clone())),
        ]);
        let all_possible_features = HashMap::from([
            (exp_pkg_a, HashSet::new()),
            (
                exp_pkg_b,
                HashSet::from([FeatureName::new("xd"), FeatureName::new("xdd")]),
            ),
        ]);
        let versions_for_location = HashMap::from([
            (exp_location_a, HashSet::from([Some(Version::new(1, 0, 0))])),
            (exp_location_b, HashSet::from([Some(Version::new(2, 0, 0))])),
        ]);
        let location_resolver =
            HashMap::from([(location_a, exp_location_a), (location_b, exp_location_b)]);

        let preexisting_packages = HashSet::from([exp_pkg_b]);
        let preexisting_features =
            HashMap::from([(exp_pkg_b, HashSet::from([FeatureName::new("xdd")]))]);

        let input = SolverInput {
            gathered_manifests,
            all_possible_features,
            versions_for_location,
            location_resolver,
            preexisting_packages,
            preexisting_features,
            preexisting_dependencies: HashMap::new(),
        };

        let main_pkg = (exp_pkg_a, HashSet::new());
        let output = SolverEngine::run_engine(input, &main_pkg).unwrap();
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let location_a = InternedLocation::new(Location::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("a"),
        });
        let location_b = InternedLocation::new(Location::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        });
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
        let gathered_manifests = HashMap::from([
            (exp_pkg_a, Box::new(manifest_a.manifest().clone())),
            (exp_pkg_b, Box::new(manifest_b.manifest().clone())),
        ]);
        let all_possible_features = HashMap::from([
            (exp_pkg_a, HashSet::new()),
            (
                exp_pkg_b,
                HashSet::from([FeatureName::new("xd"), FeatureName::new("xdd")]),
            ),
        ]);
        let versions_for_location = HashMap::from([
            (exp_location_a, HashSet::from([Some(Version::new(1, 0, 0))])),
            (exp_location_b, HashSet::from([Some(Version::new(2, 0, 0))])),
        ]);
        let location_resolver =
            HashMap::from([(location_a, exp_location_a), (location_b, exp_location_b)]);

        let preexisting_packages = HashSet::from([exp_pkg_b]);

        let input = SolverInput {
            gathered_manifests,
            all_possible_features,
            versions_for_location,
            location_resolver,
            preexisting_packages,
            preexisting_features: HashMap::new(),
            preexisting_dependencies: HashMap::new(),
        };

        let main_pkg = (exp_pkg_a, HashSet::new());
        let output = SolverEngine::run_engine(input, &main_pkg).unwrap();
        assert!(output.new_packages == HashSet::from([exp_pkg_a]));
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let manifest_c = parse_manifest(&path_c, &ctx).unwrap();
        let location_a = InternedLocation::new(Location::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("a"),
        });
        let location_b = InternedLocation::new(Location::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("b"),
        });
        let location_c = InternedLocation::new(Location::Registry {
            url: Url::parse("http://localhost:9001").unwrap(),
            real_name: StrId::from("c"),
        });
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
        let gathered_manifests = HashMap::from([
            (exp_pkg_a, Box::new(manifest_a.manifest().clone())),
            (exp_pkg_b, Box::new(manifest_b.manifest().clone())),
            (exp_pkg_c, Box::new(manifest_c.manifest().clone())),
        ]);
        let all_possible_features = HashMap::from([
            (exp_pkg_a, HashSet::new()),
            (exp_pkg_b, HashSet::from([FeatureName::new("xd")])),
            (exp_pkg_c, HashSet::from([FeatureName::new("xdd")])),
        ]);
        let versions_for_location = HashMap::from([
            (exp_location_a, HashSet::from([Some(Version::new(1, 0, 0))])),
            (exp_location_b, HashSet::from([Some(Version::new(2, 0, 0))])),
            (exp_location_c, HashSet::from([Some(Version::new(3, 0, 0))])),
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
                manifest_child_name: StrId::new("c"),
            },
            Some(Version::new(3, 0, 0)),
        )]);

        let input = SolverInput {
            gathered_manifests,
            all_possible_features,
            versions_for_location,
            location_resolver,
            preexisting_packages,
            preexisting_features: HashMap::new(),
            preexisting_dependencies,
        };

        let main_pkg = (exp_pkg_a, HashSet::new());
        let output = SolverEngine::run_engine(input, &main_pkg).unwrap();
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
