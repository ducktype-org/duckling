use std::collections::{HashMap, HashSet};

use crate::{
    QuackResult,
    quackpack::core::{
        Dependency, FeatureName, Manifest,
        solver_freeze::{PackageFreeze, VenvFreeze},
        types_common::ExpandedPackage,
    },
};

impl VenvFreeze {
    /// Finds the maximal subset of the freeze which is a correct dependency resolution,
    /// with a relaxation that main package dependencies may not be realised.
    /// Should be used as a preprocessing tool, before the freeze is passed through the solver.
    pub fn find_maximal_correct_dep_solution(
        mut self,
        manifests: &HashMap<ExpandedPackage, &Manifest>,
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
        Ok(self)
    }

    /// Helper for [`Self::find_maximal_correct_dep_solution`].
    /// Finds which packages from the freeze are not immediatelly flawed:
    ///     * we were able to obtain their manifests,
    ///     * all manifest dependencies are satisfied by appropriate freeze-written realizations.
    fn still_satisfied_pkgs(
        &self,
        manifests: &HashMap<ExpandedPackage, &Manifest>,
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
        manifests: &HashMap<ExpandedPackage, &'a Manifest>,
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
                Some(*manifest)
            } else {
                None
            }
        } else {
            Some(*manifest)
        }
    }

    /// Helper for [`Self::still_satisfied_pkgs`].
    /// Checks if a particular dependency is satisifed.
    fn check_if_dep_is_satisfied(
        freeze: &PackageFreeze,
        realization_freeze: &PackageFreeze,
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
