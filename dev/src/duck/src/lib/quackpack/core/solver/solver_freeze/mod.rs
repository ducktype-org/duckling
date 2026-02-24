mod freeze_diagnosis;
mod new_freeze_generation;

use std::{
    collections::{HashMap, HashSet},
    path::PathBuf,
};

use rand::distr::{SampleString, StandardUniform};

use crate::{
    QuackError, QuackResult, QuackResultContext, StrId, qp_bail_internal,
    quackpack::core::{
        FeatureName, Manifest,
        storage::freeze::{FreezeDep, FreezePackage, RootPackage, VenvFreeze},
        types_common::{ExpandedLocation, ExpandedPackage, InternedExpandedLocation},
    },
};

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SolverFreeze {
    pub package_freezes: HashMap<ExpandedPackage, SolverPackageFreeze>,
    pub main_pkg: ExpandedPackage,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SolverPackageFreeze {
    pub dependencies_realization: HashMap<StrId, ExpandedPackage>,
    pub features: HashSet<FeatureName>,
}

impl SolverPackageFreeze {
    pub fn new() -> Self {
        Self {
            dependencies_realization: HashMap::new(),
            features: HashSet::new(),
        }
    }
}

impl Default for SolverPackageFreeze {
    fn default() -> Self {
        Self::new()
    }
}

impl TryFrom<&VenvFreeze> for SolverFreeze {
    // @TODO: #2076 Fix issues with storage's freeze.
    type Error = QuackError;
    fn try_from(value: &VenvFreeze) -> QuackResult<Self> {
        let mut expanded_pkgs_by_name = HashMap::new();
        for pkg_freeze in value.dependencies() {
            let pkg = ExpandedPackage {
                location: pkg_freeze.source(),
                version: if pkg_freeze.source().is_registry() {
                    Some(pkg_freeze.version())
                } else {
                    None
                },
            };
            expanded_pkgs_by_name.insert(pkg_freeze.name(), pkg);
        }
        let mut pkg_freezes = HashMap::new();
        for pkg_freeze in value.dependencies() {
            let pkg = expanded_pkgs_by_name
                .get(&pkg_freeze.name())
                .context_internal("No package with given name")?;
            let mut dependencies = HashMap::new();
            for dep in pkg_freeze.dependencies() {
                let realization = expanded_pkgs_by_name
                    .get(&dep.name())
                    .context_internal("No package with given name")?;
                dependencies.insert(dep.name(), *realization);
            }
            pkg_freezes.insert(
                *pkg,
                SolverPackageFreeze {
                    dependencies_realization: dependencies,
                    features: pkg_freeze.features().iter().copied().collect(),
                },
            );
        }

        // We make the location of the main package a nonexistent one, to not mess up any freeze entries of local dependencies.
        // This assumes that there are no cyclic local dependencies.
        // The main_pkg will be corrected nonetheless.
        let main_pkg = not_existing_local_package(&pkg_freezes)?;
        let mut main_dependencies = HashMap::new();
        for dep in value.root().dependencies() {
            let realization = expanded_pkgs_by_name
                .get(&dep.name())
                .context_internal("No package with given name")?;
            main_dependencies.insert(dep.name(), *realization);
        }
        pkg_freezes.insert(
            main_pkg,
            SolverPackageFreeze {
                dependencies_realization: main_dependencies,
                features: value.root().features().iter().copied().collect(),
            },
        );

        Ok(Self {
            main_pkg,
            package_freezes: pkg_freezes,
        })
    }
}

fn not_existing_local_package(
    pkg_freezes: &HashMap<ExpandedPackage, SolverPackageFreeze>,
) -> QuackResult<ExpandedPackage> {
    for _ in 0..100 {
        let random_str = StandardUniform.sample_string(&mut rand::rng(), 16);
        let path = PathBuf::new().join(random_str);
        let pkg = ExpandedPackage {
            location: InternedExpandedLocation::new(ExpandedLocation::Local {
                absolute_path: path,
            }),
            version: None,
        };
        if !pkg_freezes.contains_key(&pkg) {
            return Ok(pkg);
        }
    }
    qp_bail_internal!("Failed to generate a fresh package")
}

impl SolverFreeze {
    pub fn empty_with_random_root() -> QuackResult<Self> {
        Ok(Self {
            main_pkg: not_existing_local_package(&HashMap::new())?,
            package_freezes: HashMap::new(),
        })
    }

    pub fn generate_storage_freeze(
        self,
        manifests: &HashMap<ExpandedPackage, Box<Manifest>>,
    ) -> QuackResult<VenvFreeze> {
        let mut pkg_freezes = vec![];
        let root_freeze = self
            .package_freezes
            .get(&self.main_pkg)
            .context_internal("No main freeze")?
            .clone();
        for (pkg, freeze) in self.package_freezes {
            let package_manifest = manifests.get(&pkg).context_internal("No main manifest")?;
            let mut dependencies = vec![];
            for (_, realization) in freeze.dependencies_realization {
                let realization_manifest = manifests
                    .get(&realization)
                    .context_internal("No manifest for realization")?;
                dependencies.push(FreezeDep::new(
                    realization_manifest.root_description().name(),
                    realization_manifest.root_description().version(),
                ));
            }
            pkg_freezes.push(FreezePackage::new(
                package_manifest.root_description().name(),
                package_manifest.root_description().version(),
                freeze.features.into_iter().collect(),
                dependencies,
                pkg.location,
            ));
        }
        let mut root_deps = vec![];
        let root_manifest = manifests
            .get(&self.main_pkg)
            .context_internal("No main manifest")?;
        for (_, realization) in root_freeze.dependencies_realization {
            let realization_manifest = manifests
                .get(&realization)
                .context_internal("No manifest for realization")?;
            root_deps.push(FreezeDep::new(
                realization_manifest.root_description().name(),
                realization_manifest.root_description().version(),
            ));
        }
        let root = RootPackage::new(
            root_manifest.root_description().name(),
            root_manifest.root_description().version(),
            root_freeze.features.into_iter().collect(),
            root_deps,
        );
        Ok(VenvFreeze::new(root, pkg_freezes))
    }
}
