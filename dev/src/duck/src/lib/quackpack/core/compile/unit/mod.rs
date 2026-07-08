//! [`Unit`] is supposed to be all information required to invoke a single instance of duckc.

use std::env::consts::{DLL_PREFIX, DLL_SUFFIX, EXE_SUFFIX};
use std::hash::Hash;
use std::sync::Arc;

use self::graph::UnitGraph;
use super::duckc::multipackage_schema;
use crate::quackpack::core::compile::compiler_package::CompilerPackage;
use crate::quackpack::core::identity::Identity;
use crate::util::hash::sha256_string;

pub mod graph;

// Missing constants from [`std::env::consts`].
const STATIC_LIB_SUFFIX: &str = ".a";

// Duckling specific.
const DVM_SUFFIX: &str = ".dbc";

#[cfg(test)]
mod tests;

#[derive(Debug, Clone)]
/// Information required to invoke duckc once.
pub struct Unit {
    inner: Arc<UnitInner>,
}

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
/// What type of artifacts a given [`Unit`] produces.
pub enum ArtifactsType {
    /// Compile to a binary
    /// Maps to the `Native` strategy
    Binary,
    /// Compile to a library (`.dll`, `.so`, `.a`, etc)
    /// Maps to the `Native` strategy
    Library,
    /// Compile to a DVM file
    /// Maps to the `Dvm` strategy
    Dvm,
    /// This [`Unit`] is a dependency and can produce only minimal artifacts
    /// Maps to the `Lib` compilation strategy, and we'll produce only minimal archives:
    /// they might be incomplete, but linker will take care of this (when compiling the root package
    /// with [`Binary`](Self::Binary) or [`Library`](Self::Library) types).
    IsADependencyArtifact,
}

#[derive(Debug)]
struct UnitInner {
    /// An internal, but unique identifier.
    unit_id: u64,
    /// Which package we're compiling.
    package: CompilerPackage,
    /// How have we got this package.
    identity: Identity,
    /// ID's of all __direct__ dependencies of this [`Unit`].
    dependencies_by_id: Vec<u64>,
    /// What artifacts should this unit produce.
    package_type: ArtifactsType,
}

impl Unit {
    /// Create a new [`Unit`].
    pub fn new(
        unit_id: u64,
        package: CompilerPackage,
        identity: Identity,
        dependencies: Vec<u64>,
        package_type: ArtifactsType,
    ) -> Self {
        assert!(
            dependencies.is_sorted(),
            "dependencies IDs should be sorted: {:?}",
            dependencies
        );
        Self {
            inner: Arc::new(UnitInner {
                unit_id,
                package,
                identity,
                dependencies_by_id: dependencies,
                package_type,
            }),
        }
    }

    /// Get the unique ID of this [`Unit`].
    pub fn unit_id(&self) -> u64 {
        self.inner.unit_id
    }

    /// Get the root package of this [`Unit`].
    pub fn root_package(&self) -> &CompilerPackage {
        &self.inner.package
    }

    /// Get the ID's of all __direct__ dependencies of this [`Unit`].
    pub fn deps_sorted_by_unit_id(&self) -> &[u64] {
        &self.inner.dependencies_by_id
    }

    /// Get the type of produced artifacts by this [`Unit`].
    pub fn artifacts_type(&self) -> ArtifactsType {
        self.inner.package_type
    }

    /// Get the [`Identity`] of this [`Unit`].
    pub fn identity(&self) -> Identity {
        self.inner.identity
    }

    /// Get a unique (in terms of the current compilation graph) name, which can be used as a directory
    /// name for storing artifacts.
    pub fn unique_name(&self) -> String {
        // Can we trim this hash?
        let id = sha256_string(self.identity().origin().to_string());
        let name = self.root_package().package().name();
        let version = self.root_package().package().version();
        format!("{}-{}-{}", name, version, id)
    }

    /// Get the filename of the output of this [`Unit`].
    pub fn output_file_name(&self) -> String {
        let name = self.root_package().package().name();
        match self.artifacts_type() {
            ArtifactsType::Binary => format!("{}{}", name, EXE_SUFFIX),
            ArtifactsType::Library => format!("{}{}{}", DLL_PREFIX, name, DLL_SUFFIX),
            ArtifactsType::Dvm => format!("{}{}", name, DVM_SUFFIX),
            ArtifactsType::IsADependencyArtifact => {
                format!("{}{}", self.unique_name(), STATIC_LIB_SUFFIX)
            }
        }
    }

    /// Get a single [`multipackage_schema::Package`] for this [`Unit`].
    pub fn multipackage_schema_package(&self, graph: &UnitGraph) -> multipackage_schema::Package {
        let package = self.root_package().package();
        let name = package.name();
        let version = package.version();
        let features = {
            let mut features = self
                .root_package()
                .enabled_features()
                .iter()
                .copied()
                .collect::<Vec<_>>();
            features.sort();
            features
        };
        let dependencies = {
            let mut result = vec![];
            for dep_id in self.deps_sorted_by_unit_id() {
                let unit_dep = graph.unit_for(*dep_id);
                let dep_name = unit_dep.root_package().package().name();
                let dep = package
                    .manifest()
                    .dependencies()
                    .get_by_name(dep_name)
                    .unwrap_or_else(|| {
                        panic!(
                            "unit=({},{}) has dep=({},{}), but it's not in the manifest?!",
                            self.unit_id(),
                            name,
                            dep_id,
                            dep_name
                        )
                    });
                result.push(multipackage_schema::Dependency {
                    id: unit_dep.unique_name().into(),
                    alias: dep.alias(),
                });
            }
            result
        };
        multipackage_schema::Package {
            id: self.unique_name().into(),
            import_name: name,
            version,
            features,
            path_to_the_src_directory: package.src().to_path_buf(),
            dependencies,
        }
    }
}

impl PartialEq for Unit {
    fn eq(&self, other: &Self) -> bool {
        Arc::ptr_eq(&self.inner, &other.inner)
    }
}

impl Eq for Unit {}

impl Hash for Unit {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        let ptr = Arc::as_ptr(&self.inner);
        std::ptr::hash(ptr, state)
    }
}
