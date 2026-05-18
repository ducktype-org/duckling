//! Helpers for modifying an existing [`EarlyGraph`] (and its members).

use tracing::debug;

use super::*;
use crate::StrId;
use crate::quackpack::core::FeatureName;
use crate::quackpack::core::compile::MISSING_DEPENDENCY_IN_MANIFEST_MESSAGE;
use crate::util::extend::QpExtend;

impl DependencyGraph {
    /// Same as [`EarlyGraph::remove_disabled_dependencies`].
    #[tracing::instrument(skip_all)]
    pub fn remove_disabled_dependencies(&mut self, packages: &PackagesSet) {
        let mut enabled_deps = HashSet::from([self.root]);
        for (k, v) in self.graph.iter_mut() {
            let mut to_remove = HashSet::new();
            let this = packages.package(k);
            for dep in &v.dependencies {
                let is_enabled = this
                    .package()
                    .manifest()
                    .dependencies()
                    .get_by_name(dep.name())
                    .expect(MISSING_DEPENDENCY_IN_MANIFEST_MESSAGE)
                    .is_enabled_for(this.enabled_features().iter().copied());
                debug!(
                    "package `{k}` has features `{}` and dependency `{dep}` is {}",
                    this.enabled_features().iter().join(" "),
                    if is_enabled { "enabled" } else { "not enabled" }
                );
                if !is_enabled {
                    to_remove.insert(*dep);
                } else {
                    enabled_deps.insert(*dep);
                }
            }
            v.dependencies.retain(|dep| !to_remove.contains(dep));
        }
        self.graph.retain(|dep, _| enabled_deps.contains(dep));
    }
}

impl EarlyGraph {
    /// Recursively populate enabled features, starting from the root of the graph.
    #[tracing::instrument(skip_all)]
    pub fn populate_features(&mut self, root_features: &[FeatureName]) -> QuackResult<()> {
        let root_package = self.package_mut(&self.graph.root());
        let root_features = root_package.mock_add_features(root_features.iter().copied())?;
        debug!(
            "starting features of root are: `{}`",
            root_features.iter().join(" ")
        );
        let mut added_features = HashMap::from([(self.graph.root, root_features)]);

        #[tracing::instrument(skip_all)]
        fn populate_impl(
            current: FreezeDep,
            graph: &HashMap<FreezeDep, DependencyNode>,
            packages: &PackagesSet,
            added_features: &mut HashMap<FreezeDep, HashSet<StrId>>,
        ) -> QuackResult<()> {
            let this = packages.package(&current);
            let this_features = added_features.entry(current).or_default().clone();
            let node = graph
                .get(&current)
                .expect("we've verified that there are dependencies");
            for dep in &node.dependencies {
                let enabled_features = {
                    let entry_in_dep_manifest = this
                        .package()
                        .manifest()
                        .dependencies()
                        .get_by_name(dep.name())
                        .expect(MISSING_DEPENDENCY_IN_MANIFEST_MESSAGE);
                    entry_in_dep_manifest.enabled_features(this_features.iter().copied())
                };
                debug!(node = %dep, features = ?enabled_features, "populating node");
                let entry = packages.package(dep);
                let expanded_features = entry.mock_add_features(enabled_features)?;
                let dep_features = added_features.entry(*dep).or_default();
                if dep_features.is_extended_by(expanded_features) {
                    populate_impl(*dep, graph, packages, added_features)?;
                }
            }
            Ok(())
        }

        // Starting from the root we proceed in a recursive manner.
        // We look at the features of the current node and for each dependency look what features are forced.
        // If some new feature appears we transition to that dependency and repeat the procedure.
        populate_impl(
            self.graph().root,
            &self.graph().graph,
            &self.packages,
            &mut added_features,
        )?;

        for (id, pkg) in self.packages.inner.iter_mut() {
            let features = added_features.entry(*id).or_default();
            pkg.add_new_features(features.iter().copied())?;
        }
        Ok(())
    }

    /// Removes disabled dependency from the graph.
    ///
    /// Note that currently they stay as keys in [`DependencyDag`], although no [`DependencyNode`]
    /// should point at them.
    ///
    /// This method should be called __after__ [`populate_features`](Self::populate_features).
    #[tracing::instrument(skip_all)]
    pub fn remove_disabled_dependencies(&mut self) {
        self.graph.remove_disabled_dependencies(&self.packages)
    }
}
