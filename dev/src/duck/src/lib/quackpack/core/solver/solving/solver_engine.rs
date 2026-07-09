use std::collections::{HashMap, HashSet};
use std::iter::once;

use russcip::ProblemCreated;

use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
use crate::quackpack::core::solver::dependency_edge::DependencyEdge;
use crate::quackpack::core::solver::gathering::gatherer_state::GatheredInfo;
use crate::quackpack::core::solver::solver_freeze::SolverFreeze;
use crate::quackpack::core::solver::solving::solver_model::{FoundSolution, SolverModel};
use crate::quackpack::core::solver::util::get_possible_realizations;
use crate::quackpack::core::{Dependency, FeatureName, Manifest, Source, Version};
use crate::quackpack::util::with_version::WithVersion;
use crate::{QuackResult, QuackResultContext, StrId};

/// Struct with all the necessary information for the solver to be run.
#[derive(Debug)]
pub struct SolverInput {
    pub gathered_manifests: HashMap<WithVersion<FullIdentity>, Box<Manifest>>,
    pub all_possible_features: HashMap<WithVersion<FullIdentity>, HashSet<FeatureName>>,
    pub versions_for_identity: HashMap<FullIdentity, HashSet<Version>>,
    pub source_to_origin_resolver: HashMap<Source, FullOrigin>,

    pub preexisting_packages: HashSet<WithVersion<FullIdentity>>,
    pub preexisting_features: HashMap<WithVersion<FullIdentity>, HashSet<FeatureName>>,
    pub preexisting_dependencies: HashMap<DependencyEdge, Version>,
}

impl SolverInput {
    /// Creates the solver input, based on the previous freeze, its packages' manifests and information gathered
    /// in the gathering phase.
    #[tracing::instrument(skip_all)]
    pub fn from_freeze_and_gathered_info(
        prev_freeze: &SolverFreeze,
        prev_freeze_manifests: HashMap<WithVersion<FullIdentity>, Box<Manifest>>,
        gathered_info: GatheredInfo,
    ) -> Self {
        let mut gathered_manifests = gathered_info.gathered_manifests;
        for (pkg, manifest) in prev_freeze_manifests {
            if prev_freeze.package_freezes.contains_key(&pkg) {
                gathered_manifests.insert(pkg, manifest);
            }
        }
        let mut all_possible_features = gathered_info.possible_features;
        let mut versions_for_identity = gathered_info.versions_for_identity;
        let mut source_to_origin_resolver = gathered_info.source_to_origin_resolver;
        let mut preexisting_packages = HashSet::new();
        let mut preexisting_features = HashMap::new();
        let mut preexisting_dependencies = HashMap::new();
        for (pkg, freeze) in prev_freeze.package_freezes.iter() {
            all_possible_features
                .entry(*pkg)
                .or_default()
                .extend(freeze.features.iter().copied());
            versions_for_identity
                .entry(*pkg.value())
                .or_default()
                .insert(pkg.version());
            source_to_origin_resolver.insert(
                Source::canonical_source_for_origin(pkg.value().origin()),
                pkg.value().origin(),
            );
            preexisting_packages.insert(*pkg);
            preexisting_features.insert(*pkg, freeze.features.clone());
            for (dep_name, realization) in freeze.dependencies_realization.iter() {
                preexisting_dependencies.insert(
                    DependencyEdge {
                        parent: *pkg,
                        dep_identity: *realization.value(),
                        manifest_child_name: *dep_name,
                    },
                    realization.version(),
                );
            }
        }
        Self {
            gathered_manifests,
            all_possible_features,
            versions_for_identity,
            source_to_origin_resolver,
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
        main_pkg: &(WithVersion<FullIdentity>, HashSet<FeatureName>),
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
        main_pkg: &(WithVersion<FullIdentity>, HashSet<FeatureName>),
    ) -> QuackResult<FoundSolution> {
        self.create_package_variables();
        let empty_hash_set: HashSet<FeatureName> = HashSet::new();
        for (package, manifest) in self.input.gathered_manifests.iter() {
            let possible_features = self
                .input
                .all_possible_features
                .get(package)
                .unwrap_or(&empty_hash_set);
            for dependency in manifest.dependencies().all_dependencies() {
                if dependency.is_enabled_for(possible_features.iter().cloned()) {
                    self.construct_for_single_dependency(*package, dependency)?;
                }
            }
        }

        self.model.require_package(main_pkg.0)?;
        for feature in main_pkg.1.iter() {
            self.model
                .require_package_with_feature(main_pkg.0, *feature)?;
        }
        self.force_features_expansion()?;
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

    /// Creates the necessary constraints for a single dependency.
    fn construct_for_single_dependency(
        &mut self,
        parent: WithVersion<FullIdentity>,
        manifest_dependency: &Dependency,
    ) -> QuackResult<()> {
        let Some(edge) = DependencyEdge::from_manifest_and_parent(
            parent,
            manifest_dependency,
            &self.input.source_to_origin_resolver,
        ) else {
            // We could not translate the manifest entry into an identity of the dependency,
            // so there are no possible realizations and we must forbid the parent package/its features
            // forcing the dependency.
            return self.forbid_forcing_features(parent, manifest_dependency)
        };

        // If this edge was not resolved in the previous freeze, we fallback to adding all constraints.
        let Some(realization_ver) = self.input.preexisting_dependencies.get(&edge) else {
            return self.add_constraints_for_edge(edge, manifest_dependency);
        };
        let realisation = WithVersion::new(edge.dep_identity, *realization_ver);

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
            .get(&parent)
            .iter()
            .cloned()
            .flatten()
        {
            if let Some(parent_preexisting) = self.input.preexisting_features.get(&parent)
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
                return self.add_constraints_for_edge(edge, manifest_dependency);
            } else {
                forcing.push((*parent_feature, forced));
            }
        }
        // If (*) never happened, we just add conditions that parent feature forces some new realisation features.
        self.model
            .require_satisfying_dep_feature_for_preexisting(parent, realisation, forcing)
    }

    /// Creates all standard constraints for a dependency edge.
    fn add_constraints_for_edge(
        &mut self,
        edge: DependencyEdge,
        manifest_dependency: &Dependency,
    ) -> QuackResult<()> {
        let possible_realizations = get_possible_realizations(
            manifest_dependency,
            &self.input.versions_for_identity,
            &self.input.source_to_origin_resolver,
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

    /// Creates version realization constraints and necessary variables for a single dependency.
    fn create_dependency_version_realization_conditions(
        &mut self,
        edge: DependencyEdge,
        manifest_dependency: &Dependency,
        possible_realizations: &[WithVersion<FullIdentity>],
    ) -> QuackResult<()> {
        for realization in possible_realizations {
            self.model
                .add_dependency_version_realisation_var(edge, realization.version());
        }

        let is_dep_forced_default = manifest_dependency.is_enabled_for(vec![]);
        if is_dep_forced_default {
            self.model.require_satisfying_dep_version(edge, None)?;
        } else {
            for dep_forcing_feature in manifest_dependency.enabling_features() {
                self.model
                    .require_satisfying_dep_version(edge, Some(*dep_forcing_feature))?;
            }
        }
        Ok(())
    }

    /// Creates feature realization constraints and necessary variables for a single dependency.
    fn create_dependency_feature_realization_conditions(
        &mut self,
        edge: DependencyEdge,
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
                .require_satisfying_dep_feature(edge, parent_feature.copied(), forced)?;
        }
        Ok(())
    }

    fn forbid_forcing_features(
        &mut self,
        parent: WithVersion<FullIdentity>,
        manifest_dependency: &Dependency
    ) -> QuackResult<()> {
        let is_dep_forced_default = manifest_dependency.is_enabled_for(vec![]);
        if is_dep_forced_default {
            self.model.forbid_package(parent)?;
        } else {
            for dep_forcing_feature in manifest_dependency.enabling_features() {
                self.model.forbid_package_with_feature(parent, *dep_forcing_feature)?;
            }
        }
        Ok(())
    }

    /// For all not previous-freeze present features adds constraints for features expansion
    /// (the presence of expandable feature forces the presence of expanded feature).
    /// Note: The constraints are added only if the feature expands to something more than itself.
    fn force_features_expansion(&mut self) -> QuackResult<()> {
        for (pkg, features) in self.input.all_possible_features.iter() {
            for feature in features {
                if let Some(preexisting) = self.input.preexisting_features.get(pkg)
                    && preexisting.contains(feature)
                {
                    continue;
                }
                let manifest = self
                    .input
                    .gathered_manifests
                    .get(pkg)
                    .context_internal("Package without manifest")?;
                let mut expanded = manifest
                    .features()
                    .expand_features(once(*feature))
                    .context_internal("No such feature")?;
                expanded.remove(feature);
                if !expanded.is_empty() {
                    self.model
                        .require_features_expansion(*pkg, *feature, expanded)?;
                }
            }
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
    edge: DependencyEdge,
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

    use super::*;
    use crate::DuckContext;
    use crate::quackpack::core::parse_manifest;
    use crate::quackpack::util::to_url::ToUrl;
    use crate::util::path_ops_ext::PathOpsExt;

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
        let ctx = DuckContext::default();
        let registry_url = "http://localhost:9001".to_url().unwrap();
        let registry_origin = FullOrigin::for_registry(registry_url.clone());
        let registry_source = Source::for_registry(registry_url);
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = WithVersion::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = WithVersion::new(identity_b, Version::new(2, 0, 0));
        let gathered_manifests = HashMap::from([
            (pkg_a, Box::new(manifest_a.manifest().clone())),
            (pkg_b, Box::new(manifest_b.manifest().clone())),
        ]);
        let all_possible_features =
            HashMap::from([(pkg_a, HashSet::new()), (pkg_b, HashSet::new())]);
        let versions_for_identity = HashMap::from([
            (identity_a, HashSet::from([Version::new(1, 0, 0)])),
            (identity_b, HashSet::from([Version::new(2, 0, 0)])),
        ]);
        let source_to_origin_resolver = HashMap::from([(registry_source, registry_origin)]);

        let input = SolverInput {
            gathered_manifests,
            all_possible_features,
            versions_for_identity,
            source_to_origin_resolver,
            preexisting_packages: HashSet::new(),
            preexisting_features: HashMap::new(),
            preexisting_dependencies: HashMap::new(),
        };

        let main_pkg = (pkg_a, HashSet::new());
        let output = SolverEngine::run_engine(input, &main_pkg).unwrap();
        assert_eq!(output.new_packages, HashSet::from([pkg_a, pkg_b]));
        assert!(output.new_features.is_empty());
        assert_eq!(
            output.new_edges,
            HashMap::from([(
                DependencyEdge {
                    parent: pkg_a,
                    dep_identity: identity_b,
                    manifest_child_name: StrId::new("b"),
                },
                Version::new(2, 0, 0)
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
    features: [xd]
"#,
        );
        let ctx = DuckContext::default();
        let registry_url = "http://localhost:9001".to_url().unwrap();
        let registry_origin = FullOrigin::for_registry(registry_url.clone());
        let registry_source = Source::for_registry(registry_url);
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = WithVersion::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = WithVersion::new(identity_b, Version::new(2, 0, 0));
        let gathered_manifests = HashMap::from([
            (pkg_a, Box::new(manifest_a.manifest().clone())),
            (pkg_b, Box::new(manifest_b.manifest().clone())),
        ]);
        let all_possible_features = HashMap::from([
            (pkg_a, HashSet::from([FeatureName::new("xd")])),
            (pkg_b, HashSet::new()),
        ]);
        let versions_for_identity = HashMap::from([
            (identity_a, HashSet::from([Version::new(1, 0, 0)])),
            (identity_b, HashSet::from([Version::new(2, 0, 0)])),
        ]);
        let source_to_origin_resolver = HashMap::from([(registry_source, registry_origin)]);
        let input = SolverInput {
            gathered_manifests,
            all_possible_features,
            versions_for_identity,
            source_to_origin_resolver,
            preexisting_packages: HashSet::new(),
            preexisting_features: HashMap::new(),
            preexisting_dependencies: HashMap::new(),
        };

        let main_pkg = (pkg_a, HashSet::new());
        let output = SolverEngine::run_engine(input, &main_pkg).unwrap();
        assert_eq!(output.new_packages, HashSet::from([pkg_a, pkg_b]));
        assert_eq!(
            output.new_features,
            HashMap::from([(pkg_a, HashSet::from([FeatureName::new("xd")]))])
        );
        assert_eq!(
            output.new_edges,
            HashMap::from([
                (
                    DependencyEdge {
                        parent: pkg_a,
                        dep_identity: identity_b,
                        manifest_child_name: StrId::new("b"),
                    },
                    Version::new(2, 0, 0)
                ),
                (
                    DependencyEdge {
                        parent: pkg_b,
                        dep_identity: identity_a,
                        manifest_child_name: StrId::new("a"),
                    },
                    Version::new(1, 0, 0)
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
  xdd: []
"#,
        );
        let ctx = DuckContext::default();
        let registry_url = "http://localhost:9001".to_url().unwrap();
        let registry_origin = FullOrigin::for_registry(registry_url.clone());
        let registry_source = Source::for_registry(registry_url);
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = WithVersion::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = WithVersion::new(identity_b, Version::new(2, 0, 0));
        let gathered_manifests = HashMap::from([
            (pkg_a, Box::new(manifest_a.manifest().clone())),
            (pkg_b, Box::new(manifest_b.manifest().clone())),
        ]);
        let all_possible_features = HashMap::from([
            (pkg_a, HashSet::new()),
            (
                pkg_b,
                HashSet::from([FeatureName::new("xd"), FeatureName::new("xdd")]),
            ),
        ]);
        let versions_for_identity = HashMap::from([
            (identity_a, HashSet::from([Version::new(1, 0, 0)])),
            (identity_b, HashSet::from([Version::new(2, 0, 0)])),
        ]);
        let source_to_origin_resolver = HashMap::from([(registry_source, registry_origin)]);

        let preexisting_packages = HashSet::from([pkg_b]);
        let preexisting_features =
            HashMap::from([(pkg_b, HashSet::from([FeatureName::new("xdd")]))]);

        let input = SolverInput {
            gathered_manifests,
            all_possible_features,
            versions_for_identity,
            source_to_origin_resolver,
            preexisting_packages,
            preexisting_features,
            preexisting_dependencies: HashMap::new(),
        };

        let main_pkg = (pkg_a, HashSet::new());
        let output = SolverEngine::run_engine(input, &main_pkg).unwrap();
        assert_eq!(output.new_packages, HashSet::from([pkg_a]));
        assert_eq!(
            output.new_features,
            HashMap::from([(pkg_b, HashSet::from([FeatureName::new("xd")]))])
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
  xdd: []
"#,
        );
        let ctx = DuckContext::default();
        let registry_url = "http://localhost:9001".to_url().unwrap();
        let registry_origin = FullOrigin::for_registry(registry_url.clone());
        let registry_source = Source::for_registry(registry_url);
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = WithVersion::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = WithVersion::new(identity_b, Version::new(2, 0, 0));
        let gathered_manifests = HashMap::from([
            (pkg_a, Box::new(manifest_a.manifest().clone())),
            (pkg_b, Box::new(manifest_b.manifest().clone())),
        ]);
        let all_possible_features = HashMap::from([
            (pkg_a, HashSet::new()),
            (
                pkg_b,
                HashSet::from([FeatureName::new("xd"), FeatureName::new("xdd")]),
            ),
        ]);
        let versions_for_identity = HashMap::from([
            (identity_a, HashSet::from([Version::new(1, 0, 0)])),
            (identity_b, HashSet::from([Version::new(2, 0, 0)])),
        ]);
        let source_to_origin_resolver = HashMap::from([(registry_source, registry_origin)]);

        let preexisting_packages = HashSet::from([pkg_b]);

        let input = SolverInput {
            gathered_manifests,
            all_possible_features,
            versions_for_identity,
            source_to_origin_resolver,
            preexisting_packages,
            preexisting_features: HashMap::new(),
            preexisting_dependencies: HashMap::new(),
        };

        let main_pkg = (pkg_a, HashSet::new());
        let output = SolverEngine::run_engine(input, &main_pkg).unwrap();
        assert_eq!(output.new_packages, HashSet::from([pkg_a]));
        assert_eq!(
            output.new_features,
            HashMap::from([(pkg_b, HashSet::from([FeatureName::new("xd")]))])
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
    features: [xd]
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
        let ctx = DuckContext::default();
        let registry_url = "http://localhost:9001".to_url().unwrap();
        let registry_origin = FullOrigin::for_registry(registry_url.clone());
        let registry_source = Source::for_registry(registry_url);
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let manifest_c = parse_manifest(&path_c, &ctx).unwrap();
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let identity_c = FullIdentity::new("c".into(), registry_origin);
        let pkg_a = WithVersion::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = WithVersion::new(identity_b, Version::new(2, 0, 0));
        let pkg_c = WithVersion::new(identity_c, Version::new(3, 0, 0));
        let gathered_manifests = HashMap::from([
            (pkg_a, Box::new(manifest_a.manifest().clone())),
            (pkg_b, Box::new(manifest_b.manifest().clone())),
            (pkg_c, Box::new(manifest_c.manifest().clone())),
        ]);
        let all_possible_features = HashMap::from([
            (pkg_a, HashSet::new()),
            (pkg_b, HashSet::from([FeatureName::new("xd")])),
            (pkg_c, HashSet::from([FeatureName::new("xdd")])),
        ]);
        let versions_for_identity = HashMap::from([
            (identity_a, HashSet::from([Version::new(1, 0, 0)])),
            (identity_b, HashSet::from([Version::new(2, 0, 0)])),
            (identity_c, HashSet::from([Version::new(3, 0, 0)])),
        ]);
        let source_to_origin_resolver = HashMap::from([(registry_source, registry_origin)]);

        let preexisting_packages = HashSet::from([pkg_b, pkg_c]);
        let preexisting_dependencies = HashMap::from([(
            DependencyEdge {
                parent: pkg_b,
                dep_identity: identity_c,
                manifest_child_name: StrId::new("c"),
            },
            Version::new(3, 0, 0),
        )]);

        let input = SolverInput {
            gathered_manifests,
            all_possible_features,
            versions_for_identity,
            source_to_origin_resolver,
            preexisting_packages,
            preexisting_features: HashMap::new(),
            preexisting_dependencies,
        };

        let main_pkg = (pkg_a, HashSet::new());
        let output = SolverEngine::run_engine(input, &main_pkg).unwrap();
        assert_eq!(output.new_packages, HashSet::from([pkg_a]));
        assert_eq!(
            output.new_features,
            HashMap::from([
                (pkg_b, HashSet::from([FeatureName::new("xd")])),
                (pkg_c, HashSet::from([FeatureName::new("xdd")]))
            ])
        )
    }

    #[test]
    /// Tests implication of form `a` requires `b` with `f`, which expands to `g`, with `a` required.
    fn feature_expansion() {
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: '1'

dependencies:
  b:
    version: '2'
    features: [f]
"#,
        );
        let (_dir_b, path_b) = prepare_manifest(
            r#"
metadata:
  name: b
  version: '2'

features:
  f: [g]
  g: []
"#,
        );
        let ctx = DuckContext::default();
        let registry_url = "http://localhost:9001".to_url().unwrap();
        let registry_origin = FullOrigin::for_registry(registry_url.clone());
        let registry_source = Source::for_registry(registry_url);
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap();
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = WithVersion::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = WithVersion::new(identity_b, Version::new(2, 0, 0));
        let gathered_manifests = HashMap::from([
            (pkg_a, Box::new(manifest_a.manifest().clone())),
            (pkg_b, Box::new(manifest_b.manifest().clone())),
        ]);
        let all_possible_features = HashMap::from([
            (pkg_a, HashSet::new()),
            (pkg_b, ["f".into(), "g".into()].into()),
        ]);
        let versions_for_identity = HashMap::from([
            (identity_a, HashSet::from([Version::new(1, 0, 0)])),
            (identity_b, HashSet::from([Version::new(2, 0, 0)])),
        ]);
        let source_to_origin_resolver = HashMap::from([(registry_source, registry_origin)]);

        let input = SolverInput {
            gathered_manifests,
            all_possible_features,
            versions_for_identity,
            source_to_origin_resolver,
            preexisting_packages: HashSet::new(),
            preexisting_features: HashMap::new(),
            preexisting_dependencies: HashMap::new(),
        };

        let main_pkg = (pkg_a, HashSet::new());
        let output = SolverEngine::run_engine(input, &main_pkg).unwrap();
        assert_eq!(output.new_packages, HashSet::from([pkg_a, pkg_b]));
        assert_eq!(
            output.new_features,
            [(pkg_b, ["f".into(), "g".into()].into()),].into()
        );
        assert_eq!(
            output.new_edges,
            HashMap::from([(
                DependencyEdge {
                    parent: pkg_a,
                    dep_identity: identity_b,
                    manifest_child_name: StrId::new("b"),
                },
                Version::new(2, 0, 0)
            )])
        );
    }
}
