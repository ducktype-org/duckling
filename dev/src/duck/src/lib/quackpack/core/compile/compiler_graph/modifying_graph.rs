use tracing::debug;

use crate::quackpack::core::FeatureName;

use super::*;

impl DependencyNode {
    pub fn populate_features(&self, packages: &AllPackages) -> QuackResult<()> {
        debug!("locking node `{}` for reading", self.node);
        let this = packages.package(self.node)?.read().expect("panick'ed");
        for dep in &self.dependencies {
            let mut entry = packages.package(dep.node)?.write().expect("panick'ed");
            debug!("locking node's dep `{}` for writing", dep.node);
            let enabled_features = {
                let entry_in_dep_manifest = this
                    .package()
                    .manifest()
                    .dependencies()
                    .get_dependency(dep.node.name())
                    .ok_or_else(|| {
                        qp_internal!(
                            "dependency `{}` was in a freezefile, but not in a manifest of `{}`?!",
                            dep.node,
                            this.package().as_freeze_dep()
                        )
                    })?;
                entry_in_dep_manifest.enabled_features(this.enabled_features().iter().copied())
            };
            entry.add_new_features(enabled_features)?;
            // Release the writer lock.
            drop(entry);
            dep.populate_features(packages)?;
        }
        Ok(())
    }

    pub fn remove_disabled_dependencies(&mut self, packages: &AllPackages) -> QuackResult<()> {
        let mut to_remove = HashSet::new();
        let this = packages.package(self.node)?.read().expect("panick'ed");
        for dep in &self.dependencies {
            let is_enabled = this
                .package()
                .manifest()
                .dependencies()
                .get_dependency(dep.node.name())
                .ok_or_else(|| {
                    qp_internal!(
                        "dependency `{}` was in a freezefile, but not in a manifest of `{}`?!",
                        dep.node,
                        this.package().as_freeze_dep()
                    )
                })?
                .is_enabled_for(this.enabled_features().iter().copied());
            if !is_enabled {
                to_remove.insert(dep.node());
            }
        }
        self.dependencies
            .retain(|dep| !to_remove.contains(&dep.node));
        for dep in &mut self.dependencies {
            dep.remove_disabled_dependencies(packages)?;
        }
        Ok(())
    }
}

impl CompilerGraph {
    pub fn populate_features(&self, root_features: &[FeatureName]) -> QuackResult<()> {
        {
            let lock = self.package(self.graph.root().node())?;
            debug!("locking root `{}` for writing", self.graph.root().node());
            let mut root_package = lock.write().expect("panick'ed");
            root_package.add_new_features(root_features.iter().copied())?;
        }
        self.graph.root.populate_features(&self.all_packages)
    }

    pub fn remove_disabled_dependencies(&mut self) -> QuackResult<()> {
        self.graph
            .root
            .remove_disabled_dependencies(&self.all_packages)
    }
}
