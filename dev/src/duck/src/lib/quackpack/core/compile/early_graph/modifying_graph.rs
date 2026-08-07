//! Helpers for modifying an existing [`EarlyGraph`] (and its members).

use std::collections::VecDeque;

use tracing::debug;

use super::{DependencyGraph, EarlyGraph, HashMap, HashSet, Identity, PackagesSet, QuackResult};
use crate::quackpack::core::FeatureName;
use crate::quackpack::core::compile::missing_depenendcy_in_manifest;
use crate::util::extend::QpExtend;

impl DependencyGraph {
    /// Same as [`EarlyGraph::remove_disabled_dependencies`], but return enabled dependencies.
    #[tracing::instrument(skip_all)]
    fn remove_disabled_dependencies(&mut self, packages: &PackagesSet) -> HashSet<Identity> {
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
                    .unwrap_or_else(|| {
                        missing_depenendcy_in_manifest(&this.package().name(), &dep.name(), this)
                    })
                    .is_enabled_for(this.enabled_features().iter().copied());
                debug!(
                    %k,
                    features = ?this.enabled_features(),
                    %dep,
                    dep_enabled = %is_enabled,
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
        enabled_deps
    }
}

impl EarlyGraph {
    /// Recursively populate enabled features, starting from the root of the graph.
    #[tracing::instrument(skip_all)]
    pub(super) fn populate_features(&mut self, root_features: &[FeatureName]) -> QuackResult<()> {
        let root_package = self.package_mut(&self.graph.root());
        let root_features =
            root_package.features_that_would_be_added(root_features.iter().copied())?;
        debug!(?root_features, "starting to expand features");
        let mut added_features = HashMap::from([(self.graph.root, root_features)]);
        let mut stack = VecDeque::from([self.graph().root]);

        // This is an iterative DFS.
        // For a given current package `current`, we iterate over its dependencies.
        // For each one we check whether the current features of `current` force some new features.
        // If so, we add that dependency to the stack.
        while let Some(current) = stack.pop_back() {
            let this = self.packages.package(&current);
            let node = self
                .graph
                .graph
                .get(&current)
                .expect("we've verified that there are dependencies");
            for dep in &node.dependencies {
                let this_features = added_features.entry(current).or_default();
                let enabled_features = {
                    let entry_in_dep_manifest = this
                        .package()
                        .manifest()
                        .dependencies()
                        .get_by_name(dep.name())
                        .unwrap_or_else(|| {
                            missing_depenendcy_in_manifest(
                                &this.package().name(),
                                &dep.name(),
                                this,
                            )
                        });
                    entry_in_dep_manifest.enabled_features(this_features.iter().copied())
                };
                debug!(node = %dep, features = ?enabled_features, "adding features to node");
                let entry = self.packages.package(dep);
                let expanded_features = entry.features_that_would_be_added(enabled_features)?;
                let dep_features = added_features.entry(*dep).or_default();
                if dep_features.extend_and_get_diff_size(expanded_features) > 0 {
                    stack.push_back(*dep);
                }
            }
        }

        for (id, pkg) in self.packages.inner.iter_mut() {
            let features = added_features.entry(*id).or_default();
            pkg.add_new_features(features.iter().copied())?;
        }
        Ok(())
    }

    /// Removes disabled dependency from the graph.
    ///
    /// Note that currently they stay as keys in [`DependencyGraph`], although no [`DependencyNode`]
    /// should point at them.
    ///
    /// This method should be called __after__ [`populate_features`](Self::populate_features).
    #[tracing::instrument(skip_all)]
    pub(super) fn remove_disabled_dependencies(&mut self) {
        let enabled_deps = self.graph.remove_disabled_dependencies(&self.packages);
        // Also clear identity cache.
        self.packages
            .inner
            .retain(|dep, _| enabled_deps.contains(dep));
    }
}
