use tracing::debug;

use crate::quackpack::core::FeatureName;

use super::*;

impl DependencyDag {
    pub fn remove_disabled_dependencies(&mut self, packages: &AllPackages) -> QuackResult<()> {
        for (k, v) in self.dag.iter_mut() {
            let mut to_remove = HashSet::new();
            let this = packages.package(k)?;
            for dep in &v.dependencies {
                let is_enabled = this
                    .package()
                    .manifest()
                    .dependencies()
                    .get_dependency(dep.name())
                    .with_context_internal(|| {
                        format!(
                            "dependency `{}` was in a freezefile, but not in a manifest of `{}`?!",
                            dep,
                            this.package().as_freeze_dep()
                        )
                    })?
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
        Ok(())
    }
}

impl CompilerDag {
    pub fn populate_features(&mut self, root_features: &[FeatureName]) -> QuackResult<()> {
        let root_package = self.package_mut(&self.dag.root())?;
        root_package.add_new_features(root_features.iter().copied())?;

        fn visit_impl(
            current: FreezeDep,
            dag: &HashMap<FreezeDep, DependencyNode>,
            packages: &mut AllPackages,
        ) -> QuackResult<()> {
            let this = packages.package(&current)?;
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
                        .get_dependency(dep.name())
                        .with_context_internal(|| {
                            format!(
                                "dependency `{}` was in a freezefile, but not in a manifest of `{}`?!",
                                dep,
                                this.as_freeze_dep()
                            )
                        })?;
                    entry_in_dep_manifest.enabled_features(this_features.iter().copied())
                };
                let entry = packages.package_mut(dep)?;
                entry.add_new_features(enabled_features)?;
                visit_impl(*dep, dag, packages)?;
            }
            Ok(())
        }

        visit_impl(self.dag.root(), &self.dag.dag, &mut self.all_packages)
    }

    pub fn remove_disabled_dependencies(&mut self) -> QuackResult<()> {
        self.dag.remove_disabled_dependencies(&self.all_packages)
    }
}
