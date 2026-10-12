// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Helpers for modifying an existing [`EarlyGraph`] (and its members).

use std::collections::VecDeque;

use tracing::debug;

use super::{DependencyGraph, EarlyGraph, HashMap, HashSet, PackagesSet, QuackResult};
use crate::quackpack::core::FeatureName;
use crate::quackpack::core::compile::missing_depenendcy_in_manifest;
use crate::util::extend::QpExtend;

impl DependencyGraph {
    /// Same as [`EarlyGraph::remove_disabled_dependencies`].
    #[tracing::instrument(skip_all)]
    fn remove_disabled_dependencies(&mut self, packages: &PackagesSet) {
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
                    .is_enabled_for(this.enabled_features());
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
    }

    /// Remove unreachable dependencies.
    ///
    /// When removing disabled dependencies, we might leave some unreachable ones. Get rid of them too.
    #[tracing::instrument(skip_all)]
    fn remove_unreachable_dependencies(&mut self) {
        let mut reachable_deps = HashSet::new();
        let mut bfs_stack = VecDeque::from([self.root]);
        while let Some(current) = bfs_stack.pop_front() {
            let inserted_new_node = reachable_deps.insert(current);
            if !inserted_new_node {
                continue;
            }
            let deps = self.dependencies_for_package(&current).dependencies();
            bfs_stack.extend(deps);
        }
        self.graph.retain(|dep, _| {
            if reachable_deps.contains(dep) {
                return true;
            }
            debug!(?dep, "removing from the graph as it's unreachable");
            false
        });
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
        let mut enabled_deps = HashSet::from([self.graph.root]);
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
                let (enabled_features, is_enabled) = {
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
                    (
                        entry_in_dep_manifest.enabled_features(this_features),
                        entry_in_dep_manifest.is_enabled_for(this_features),
                    )
                };
                let became_available = if is_enabled {
                    enabled_deps.insert(*dep)
                } else {
                    false
                };
                debug!(node = %dep, features = ?enabled_features, was_enabled = %is_enabled, %became_available, "adding features to node");
                // We shouldn't modify feature flags of disabled dependencies.
                if !is_enabled {
                    continue;
                }

                let entry = self.packages.package(dep);
                let expanded_features = entry.features_that_would_be_added(enabled_features)?;
                let dep_features = added_features.entry(*dep).or_default();
                let new_features_count = dep_features.extend_and_get_diff_size(expanded_features);
                // We should visit dependency if it is enabled and either of two things have happened:
                // * it just became enabled or,
                // * its feature flags have changed.
                let should_visit = became_available || new_features_count > 0;
                if should_visit {
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
    /// This method should be called __after__ [`populate_features`](Self::populate_features).
    #[tracing::instrument(skip_all)]
    pub(super) fn remove_disabled_dependencies(&mut self) {
        self.graph.remove_disabled_dependencies(&self.packages);
        self.graph.remove_unreachable_dependencies();
        // Also clear identity cache.
        self.packages.inner.retain(|dep, _| {
            if self.graph.graph.contains_key(dep) {
                return true;
            }
            debug!(
                ?dep,
                "removing from the PackagesSet, as it's unreachable or disabled"
            );
            false
        });
    }
}
