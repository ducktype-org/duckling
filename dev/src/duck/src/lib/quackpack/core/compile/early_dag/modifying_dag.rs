//! Helpers for modifying an existing [`EarlyDag`] (and its members).

use tracing::debug;

use super::*;
use crate::quackpack::core::FeatureName;
use crate::quackpack::core::compile::MISSING_DEPENDENCY_IN_MANIFEST_MESSAGE;

impl DependencyDag {
    /// Same as [`EarlyDag::remove_disabled_dependencies`].
    #[tracing::instrument(skip_all)]
    pub fn remove_disabled_dependencies(&mut self, packages: &PackagesSet) {
        for (k, v) in self.dag.iter_mut() {
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
                }
            }
            v.dependencies.retain(|dep| !to_remove.contains(dep));
        }
    }
}

impl EarlyDag {
    /// Recursively populate enabled features, starting from the root of the graph.
    #[tracing::instrument(skip_all)]
    pub fn populate_features(&mut self, root_features: &[FeatureName]) -> QuackResult<()> {
        debug!(root = %self.dag.root(), features = ?root_features, "populating root");
        let root_package = self.package_mut(&self.dag.root());
        root_package.add_new_features(root_features.iter().copied())?;

        #[tracing::instrument(skip_all)]
        fn populate_impl(
            current: FreezeDep,
            dag: &HashMap<FreezeDep, DependencyNode>,
            packages: &mut PackagesSet,
        ) -> QuackResult<()> {
            let this = packages.package(&current);
            let this_features = this.enabled_features().clone();
            let this = this.package().clone();
            let node = dag
                .get(&current)
                .expect("we've verified that there are dependencies");
            for dep in &node.dependencies {
                let enabled_features = {
                    let entry_in_dep_manifest = this
                        .manifest()
                        .dependencies()
                        .get_by_name(dep.name())
                        .expect(MISSING_DEPENDENCY_IN_MANIFEST_MESSAGE);
                    entry_in_dep_manifest.enabled_features(this_features.iter().copied())
                };
                debug!(node = %dep, features = ?enabled_features, "populating node");
                let entry = packages.package_mut(dep);
                entry.add_new_features(enabled_features)?;
            }
            Ok(())
        }

        let order = self
            .dag
            .topo_sort_order()
            .expect("we've verified that there are no cycles");
        for dep in order {
            populate_impl(dep, &self.dag.dag, &mut self.packages)?;
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
        self.dag.remove_disabled_dependencies(&self.packages)
    }
}
