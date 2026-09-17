//! Main logic for solving dependencies.
//! Contains [`SolverEngine`] struct, which transforms the problem into an integer linear programming instance and solves it.
use std::collections::{HashMap, HashSet};
use std::iter::once;

use russcip::ProblemCreated;

use crate::quackpack::core::full_identity::FullIdentity;
use crate::quackpack::core::solver::dependency_edge::DependencyEdge;
use crate::quackpack::core::solver::solving::input::{PackageData, SolverInput};
use crate::quackpack::core::solver::solving::solver_model::{FoundSolution, SolverModel};
use crate::quackpack::core::solver::util::get_possible_realizations;
use crate::quackpack::core::{Dependency, FeatureName, Manifest, PackageId, Version};
use crate::{QuackResult, QuackResultContext, StrId};

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
    pub(in crate::quackpack::core::solver) fn run_engine(
        input: SolverInput,
        main_pkg: &(PackageId, HashSet<FeatureName>),
    ) -> QuackResult<(FoundSolution, HashMap<PackageId, Box<Manifest>>)> {
        let engine = SolverEngine::new(&input);
        let solution = engine.run(main_pkg)?;
        let manifests = input.into_manifests();
        Ok((solution, manifests))
    }

    /// Creates a new [`SolverEngine`] from the given [`SolverInput`] reference.
    fn new(input: &'a SolverInput) -> Self {
        Self {
            input,
            model: SolverModel::new(input),
        }
    }

    /// Runs the engine, building the underlying solver model, solving it and returning the output.
    fn run(mut self, main_pkg: &(PackageId, HashSet<FeatureName>)) -> QuackResult<FoundSolution> {
        self.create_package_variables();
        for (package, data) in self.input.packages_data.iter() {
            for dependency in data.manifest().dependencies().all_dependencies() {
                if dependency.is_enabled_for(data.features().iter().cloned()) {
                    self.construct_for_single_dependency(*package, data, dependency)?;
                }
            }
        }

        // Root has to be present in the solution with all its requested features.
        self.model.require_package(main_pkg.0)?;
        for feature in main_pkg.1.iter() {
            self.model
                .require_package_with_feature(main_pkg.0, *feature)?;
        }
        self.force_features_expansion()?;
        self.force_singular_versions()?;
        self.model.solve()
    }

    /// Creates necessary variables for all the packages.
    fn create_package_variables(&mut self) {
        for (pkg, data) in self.input.packages_data.iter() {
            self.model.add_package_var(*pkg);
            for feature in data.features().iter().copied() {
                self.model.add_package_with_feature_var(*pkg, feature);
            }
        }
    }

    /// Creates the necessary constraints for a single dependency.
    fn construct_for_single_dependency(
        &mut self,
        parent: PackageId,
        parent_data: &PackageData,
        manifest_dependency: &Dependency,
    ) -> QuackResult<()> {
        let Some(edge) = DependencyEdge::from_manifest_and_parent(
            parent,
            manifest_dependency,
            &self.input.source_to_origin_resolver,
        ) else {
            // We could not translate the manifest entry into an identity of the dependency,
            // so there are no possible realizations and we must forbid the dependency.
            // This can happen due to merciful mode (we failed to fetch any realizations).
            return self.forbid_dependency(parent, manifest_dependency);
        };

        // If this edge was not resolved in the previous freeze, we fallback to adding all constraints.
        let Some(realization) = parent_data.dependency_preexists(edge.manifest_child_name) else {
            return self.add_constraints_for_edge(edge, manifest_dependency);
        };
        // Below we assume that the dependency was realized in the previous freeze.
        let realization_data = self.input.package_data(realization)?;

        // Each feature of the parent may force some additional features of the child,
        // not present in the previous freeze.
        // We try to add them to the chosen realization.

        // Mapping not-preexisting parent feature -> realization features forced by it.
        let mut forcing = vec![];
        let realization_features = &realization_data.features();
        for parent_feature in parent_data.features().iter().copied() {
            if parent_data.feature_preexists(parent_feature) {
                // Parent feature belonged to the previous freeze, so whatever it forced, has been already taken care of.
                continue;
            }
            let forced = manifest_dependency.enabled_features(vec![parent_feature]);
            if forced
                .iter()
                .any(|feature| !realization_features.contains(feature))
            {
                // (*) Previously chosen realization of the dependency does not support some of the forced flags.
                // This means that we should choose another realization,
                // but doing so would result in the realization being in 2 or more versions.
                // Thus we explicitly forbid parent with the feature.
                // Note that this edge case can happen only in the merciful mode.
                return self
                    .model
                    .forbid_package_with_feature(parent, parent_feature);
            } else {
                forcing.push((parent_feature, forced));
            }
        }
        // If (*) never happened, we just add conditions that parent feature forces some new realization features.
        self.model
            .require_satisfying_dep_feature_for_preexisting(parent, realization, forcing)
    }

    /// Creates all standard constraints for a dependency edge:
    /// 1. If parent is present then this specific dependency has to have a realization chosen.
    /// 2. If parent is present (without or with features) then this dependency may force some features.
    /// 3. If for this dependency a realization was chosen, then the package of realization has to be present.
    /// 4. If some forced features were determined, then the realization has to be present with them.
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
        self.model
            .require_substantiate_dep_features(edge, &possible_realizations)?;
        Ok(())
    }

    /// Creates version realization constraints and necessary variables for a single dependency.
    fn create_dependency_version_realization_conditions(
        &mut self,
        edge: DependencyEdge,
        manifest_dependency: &Dependency,
        possible_realizations: &[PackageId],
    ) -> QuackResult<()> {
        for realization in possible_realizations {
            self.model
                .add_dependency_version_realization_var(edge, realization.version());
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
            // Features forced by the parent feature but not forced by default,
            // or features forced by default if parent_feature is None.
            let forced = match parent_feature {
                None => &enabled_always,
                Some(feature) => {
                    // We use this trick so that forced is a reference and we do not need to clone `enabled_always`.
                    tmp_hash_set =
                        HashSet::from_iter(manifest_dependency.enabled_features(vec![feature]))
                            .difference(&enabled_always)
                            .copied()
                            .collect();
                    &tmp_hash_set
                }
            };
            if forced.is_empty() {
                continue;
            }
            for feature in forced.iter() {
                self.model
                    .add_dependency_feature_realization_var(edge, *feature);
            }
            self.model
                .require_satisfying_dep_feature(edge, parent_feature, forced)?;
        }
        Ok(())
    }

    /// Assure that this dependency is never realized.
    fn forbid_dependency(
        &mut self,
        parent: PackageId,
        manifest_dependency: &Dependency,
    ) -> QuackResult<()> {
        let is_dep_forced_default = manifest_dependency.is_enabled_for(vec![]);
        if is_dep_forced_default {
            // The dependency is enabled by default, so `parent` can never be chosen.
            self.model.forbid_package(parent)?;
        } else {
            // Forbid features of `parent` which enable this dependency.
            for dep_forcing_feature in manifest_dependency.enabling_features() {
                self.model
                    .forbid_package_with_feature(parent, *dep_forcing_feature)?;
            }
        }
        Ok(())
    }

    /// For all not previous-freeze present features adds constraints for features expansion
    /// (the presence of expandable feature forces the presence of expanded feature).
    /// Note:
    /// -----
    /// The constraints are added only if the feature expands to something more than itself.
    fn force_features_expansion(&mut self) -> QuackResult<()> {
        for (pkg, data) in self.input.packages_data.iter() {
            for feature in data.features().iter() {
                if data.feature_preexists(*feature) {
                    continue;
                }
                let manifest = data.manifest();
                let mut expanded = manifest
                    .features()
                    .expand_features(once(*feature))
                    .with_context_internal(|| {
                        format!(
                            "package `{pkg:?}` does not have a feature `{feature:?}`; {manifest:#?}"
                        )
                    })?;
                expanded.remove(feature);
                if !expanded.is_empty() {
                    self.model
                        .require_features_expansion(*pkg, *feature, expanded)?;
                }
            }
        }
        Ok(())
    }

    /// Make sure that every package is present in at most one version.
    fn force_singular_versions(&mut self) -> QuackResult<()> {
        for (identity, versions) in self.input.versions_for_identity.iter() {
            self.force_singular_version(*identity, versions)?;
        }
        Ok(())
    }

    /// Make sure that the package described by `identity` is present in at most one version.
    /// Note:
    /// -----
    /// This has a different workflow, depending on whether any version is preexisting.
    /// This is necessary, since for preexisting packages we do not care whether their variables will evaluate to 0 or 1,
    /// so we can't just always add a constraint that more than many versions are prohibited.
    fn force_singular_version(
        &mut self,
        identity: FullIdentity,
        versions: &HashSet<Version>,
    ) -> QuackResult<()> {
        let mut preexistent_version = None;
        for version in versions {
            let pkg = PackageId::new(identity, *version);
            if self.input.preexists(pkg) {
                preexistent_version = Some(*version);
                break;
            }
        }
        if let Some(preexistent_version) = preexistent_version {
            self.forbid_versions_not_preexisting(identity, versions, preexistent_version)
        } else {
            self.forbid_more_than_one_version(identity, versions)
        }
    }

    /// Forbid a package being chosen in more than one version.
    fn forbid_more_than_one_version(
        &mut self,
        identity: FullIdentity,
        versions: &HashSet<Version>,
    ) -> QuackResult<()> {
        self.model.forbid_more_that_one_version(identity, versions)
    }

    /// Forbid a package in all versions except a single preexisting one.
    fn forbid_versions_not_preexisting(
        &mut self,
        identity: FullIdentity,
        versions: &HashSet<Version>,
        preexistent_version: Version,
    ) -> QuackResult<()> {
        for version in versions {
            if *version == preexistent_version {
                continue;
            }
            let pkg = PackageId::new(identity, *version);
            self.model.forbid_package(pkg)?;
        }
        Ok(())
    }
}

/// Creates an iterator of all possible parent features and None.
/// This function is used when trying to determine
/// which features of the child are forced by which features of the parent.
/// The [`None`] signifies the lack of any parent features,
/// so that features of the child forced by default can be considered.
fn parent_features_to_consider(
    input: &SolverInput,
    edge: DependencyEdge,
) -> impl Iterator<Item = Option<StrId>> {
    input
        .packages_data
        .get(&edge.parent)
        .into_iter()
        .flat_map(|data| data.features().iter().copied())
        .map(Some)
        .chain(once(None))
}

#[cfg(test)]
mod test {
    use std::path::PathBuf;

    use tempfile::{TempDir, tempdir};

    use super::*;
    use crate::DuckContext;
    use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
    use crate::quackpack::core::solver::solving::input::{PackageData, PreexistenceData};
    use crate::quackpack::core::{Source, Version, parse_manifest};
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0.into_manifest();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0.into_manifest();
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let versions_for_identity = HashMap::from([
            (identity_a, HashSet::from([Version::new(1, 0, 0)])),
            (identity_b, HashSet::from([Version::new(2, 0, 0)])),
        ]);
        let source_to_origin_resolver = HashMap::from([(registry_source, registry_origin)]);

        let packages_data = [
            (pkg_a, PackageData::new_empty(manifest_a)),
            (pkg_b, PackageData::new_empty(manifest_b)),
        ]
        .into();

        let input = SolverInput {
            packages_data,
            versions_for_identity,
            source_to_origin_resolver,
        };

        let main_pkg = (pkg_a, HashSet::new());
        let (output, _) = SolverEngine::run_engine(input, &main_pkg).unwrap();
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0.into_manifest();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0.into_manifest();
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let versions_for_identity = HashMap::from([
            (identity_a, HashSet::from([Version::new(1, 0, 0)])),
            (identity_b, HashSet::from([Version::new(2, 0, 0)])),
        ]);
        let source_to_origin_resolver = HashMap::from([(registry_source, registry_origin)]);
        let data_a = PackageData::new(manifest_a, ["xd".into()].into(), None);
        let packages_data = [(pkg_a, data_a), (pkg_b, PackageData::new_empty(manifest_b))].into();
        let input = SolverInput {
            packages_data,
            versions_for_identity,
            source_to_origin_resolver,
        };

        let main_pkg = (pkg_a, HashSet::new());
        let (output, _) = SolverEngine::run_engine(input, &main_pkg).unwrap();
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0.into_manifest();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0.into_manifest();
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let data_b = PackageData::new(
            manifest_b,
            ["xd".into(), "xdd".into()].into(),
            Some(PreexistenceData::new(["xdd".into()].into(), [].into())),
        );
        let packages_data = [(pkg_a, PackageData::new_empty(manifest_a)), (pkg_b, data_b)].into();
        let versions_for_identity = HashMap::from([
            (identity_a, HashSet::from([Version::new(1, 0, 0)])),
            (identity_b, HashSet::from([Version::new(2, 0, 0)])),
        ]);
        let source_to_origin_resolver = HashMap::from([(registry_source, registry_origin)]);

        let input = SolverInput {
            packages_data,
            versions_for_identity,
            source_to_origin_resolver,
        };

        let main_pkg = (pkg_a, HashSet::new());
        let (output, _) = SolverEngine::run_engine(input, &main_pkg).unwrap();
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0.into_manifest();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0.into_manifest();
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let data_b = PackageData::new(
            manifest_b,
            ["xd".into(), "xdd".into()].into(),
            Some(PreexistenceData::default()),
        );
        let packages_data = [(pkg_a, PackageData::new_empty(manifest_a)), (pkg_b, data_b)].into();
        let versions_for_identity = HashMap::from([
            (identity_a, HashSet::from([Version::new(1, 0, 0)])),
            (identity_b, HashSet::from([Version::new(2, 0, 0)])),
        ]);
        let source_to_origin_resolver = HashMap::from([(registry_source, registry_origin)]);

        let input = SolverInput {
            packages_data,
            versions_for_identity,
            source_to_origin_resolver,
        };

        let main_pkg = (pkg_a, HashSet::new());
        let (output, _) = SolverEngine::run_engine(input, &main_pkg).unwrap();
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0.into_manifest();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0.into_manifest();
        let manifest_c = parse_manifest(&path_c, &ctx).unwrap().0.into_manifest();
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let identity_c = FullIdentity::new("c".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let pkg_c = PackageId::new(identity_c, Version::new(3, 0, 0));
        let data_b = PackageData::new(
            manifest_b,
            ["xd".into()].into(),
            Some(PreexistenceData::new(
                [].into(),
                [("c".into(), pkg_c)].into(),
            )),
        );
        let data_c = PackageData::new(
            manifest_c,
            ["xdd".into()].into(),
            Some(PreexistenceData::default()),
        );
        let packages_data = [
            (pkg_a, PackageData::new_empty(manifest_a)),
            (pkg_b, data_b),
            (pkg_c, data_c),
        ]
        .into();
        let versions_for_identity = HashMap::from([
            (identity_a, HashSet::from([Version::new(1, 0, 0)])),
            (identity_b, HashSet::from([Version::new(2, 0, 0)])),
            (identity_c, HashSet::from([Version::new(3, 0, 0)])),
        ]);
        let source_to_origin_resolver = HashMap::from([(registry_source, registry_origin)]);

        let input = SolverInput {
            packages_data,
            versions_for_identity,
            source_to_origin_resolver,
        };

        let main_pkg = (pkg_a, HashSet::new());
        let (output, _) = SolverEngine::run_engine(input, &main_pkg).unwrap();
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0.into_manifest();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0.into_manifest();
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let data_b = PackageData::new(manifest_b, ["f".into(), "g".into()].into(), None);
        let packages_data = [(pkg_a, PackageData::new_empty(manifest_a)), (pkg_b, data_b)].into();
        let versions_for_identity = HashMap::from([
            (identity_a, HashSet::from([Version::new(1, 0, 0)])),
            (identity_b, HashSet::from([Version::new(2, 0, 0)])),
        ]);
        let source_to_origin_resolver = HashMap::from([(registry_source, registry_origin)]);

        let input = SolverInput {
            packages_data,
            versions_for_identity,
            source_to_origin_resolver,
        };

        let main_pkg = (pkg_a, HashSet::new());
        let (output, _) = SolverEngine::run_engine(input, &main_pkg).unwrap();
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

    #[test]
    /// Assures that there cannot be two versions of same package chosen.
    /// * `a` depends on `b` and `c` in version 1,
    /// * `b` depends on `c` version 2.
    /// This should end in an error, since `c` is required in both versions.
    fn conflicting_versions_error() {
        let (_dir_a, path_a) = prepare_manifest(
            r#"
metadata:
  name: a
  version: '1'

dependencies:
  b:
    version: '1'
  c:
    version: '1'
"#,
        );
        let (_dir_b, path_b) = prepare_manifest(
            r#"
metadata:
  name: b
  version: '1'

dependencies:
  c:
    version: '2'
"#,
        );
        let (_dir_c1, path_c1) = prepare_manifest(
            r#"
metadata:
  name: c
  version: '1'
"#,
        );
        let (_dir_c2, path_c2) = prepare_manifest(
            r#"
metadata:
  name: c
  version: '2'
"#,
        );
        let ctx = DuckContext::default();
        let registry_url = "http://localhost:9001".to_url().unwrap();
        let registry_origin = FullOrigin::for_registry(registry_url.clone());
        let registry_source = Source::for_registry(registry_url);
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0.into_manifest();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0.into_manifest();
        let manifest_c1 = parse_manifest(&path_c1, &ctx).unwrap().0.into_manifest();
        let manifest_c2 = parse_manifest(&path_c2, &ctx).unwrap().0.into_manifest();
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let identity_c = FullIdentity::new("c".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(1, 0, 0));
        let pkg_c1 = PackageId::new(identity_c, Version::new(1, 0, 0));
        let pkg_c2 = PackageId::new(identity_c, Version::new(2, 0, 0));

        let packages_data = [
            (pkg_a, PackageData::new_empty(manifest_a)),
            (pkg_b, PackageData::new_empty(manifest_b)),
            (pkg_c1, PackageData::new_empty(manifest_c1)),
            (pkg_c2, PackageData::new_empty(manifest_c2)),
        ]
        .into();
        let versions_for_identity = HashMap::from([
            (identity_a, HashSet::from([Version::new(1, 0, 0)])),
            (identity_b, HashSet::from([Version::new(1, 0, 0)])),
            (identity_c, HashSet::from([1.into(), 2.into()])),
        ]);
        let source_to_origin_resolver = HashMap::from([(registry_source, registry_origin)]);
        let input = SolverInput {
            packages_data,
            versions_for_identity,
            source_to_origin_resolver,
        };

        let main_pkg = (pkg_a, HashSet::new());
        let err = SolverEngine::run_engine(input, &main_pkg).unwrap_err();
        assert_eq!(err.to_string(), "failed to find a solution");
    }

    #[test]
    /// Both `a` and `b` preexisted, but with no features.
    /// Now `a` has feature `new` which forces `b` with `xd`, which is not supported by `b`.
    /// Thus it should result in an error.
    fn new_feature_of_preexisting_forces_nonexistent_error() {
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
        package-features: [new]

features:
  new: []
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
        let manifest_a = parse_manifest(&path_a, &ctx).unwrap().0.into_manifest();
        let manifest_b = parse_manifest(&path_b, &ctx).unwrap().0.into_manifest();
        let identity_a = FullIdentity::new("a".into(), registry_origin);
        let identity_b = FullIdentity::new("b".into(), registry_origin);
        let pkg_a = PackageId::new(identity_a, Version::new(1, 0, 0));
        let pkg_b = PackageId::new(identity_b, Version::new(2, 0, 0));
        let data_a = PackageData::new(
            manifest_a,
            ["new".into()].into(),
            Some(PreexistenceData::default()),
        );
        let data_b = PackageData::new(manifest_b, [].into(), Some(PreexistenceData::default()));
        let packages_data = [(pkg_a, data_a), (pkg_b, data_b)].into();
        let versions_for_identity = HashMap::from([
            (identity_a, HashSet::from([Version::new(1, 0, 0)])),
            (identity_b, HashSet::from([Version::new(2, 0, 0)])),
        ]);
        let source_to_origin_resolver = HashMap::from([(registry_source, registry_origin)]);

        let input = SolverInput {
            packages_data,
            versions_for_identity,
            source_to_origin_resolver,
        };

        let main_pkg = (pkg_a, ["new".into()].into());
        let err = SolverEngine::run_engine(input, &main_pkg).unwrap_err();
        assert_eq!(err.to_string(), "failed to find a solution");
    }
}
