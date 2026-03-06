use crate::quackpack::core::FeatureName;

use super::*;

impl DependencyNode {
    pub fn populate_features(&self, packages: &mut AllPackages) -> QuackResult<()> {
        let this = packages.package(self.node)?;
        let this_features = this.enabled_features().clone();
        let this = this.package().clone();
        for dep in &self.dependencies {
            let enabled_features = {
                let entry_in_dep_manifest = this
                    .manifest()
                    .dependencies()
                    .get_dependency(dep.node.name())
                    .ok_or_else(|| {
                        qp_internal!(
                            "dependency `{}` was in a freezefile, but not in a manifest of `{}`?!",
                            dep.node,
                            this.as_freeze_dep()
                        )
                    })?;
                entry_in_dep_manifest.enabled_features(this_features.iter().copied())
            };
            let entry = packages.package_mut(dep.node)?;
            entry.add_new_features(enabled_features)?;
            dep.populate_features(packages)?;
        }
        Ok(())
    }

    pub fn remove_disabled_dependencies(&mut self, packages: &AllPackages) -> QuackResult<()> {
        let mut to_remove = HashSet::new();
        let this = packages.package(self.node)?;
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
    pub fn populate_features(&mut self, root_features: &[FeatureName]) -> QuackResult<()> {
        let root_package = self.package_mut(self.graph.root().node())?;
        root_package.add_new_features(root_features.iter().copied())?;
        self.graph.root.populate_features(&mut self.all_packages)
    }

    pub fn remove_disabled_dependencies(&mut self) -> QuackResult<()> {
        self.graph
            .root
            .remove_disabled_dependencies(&self.all_packages)
    }
}
