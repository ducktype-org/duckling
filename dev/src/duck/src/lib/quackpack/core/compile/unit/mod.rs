//! [`Unit`] is supposed to be all information required to invoke a single instance of duckc.

use std::sync::Arc;

use crate::quackpack::core::compile::compiler_package::CompilerPackage;
use crate::quackpack::core::storage::package_id::PackageId;
use crate::util::hash::sha256_string;

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
    id: u64,
    /// Which package we're compiling.
    package: CompilerPackage,
    /// How have we got this package.
    storage_id: PackageId,
    /// ID's of all __direct__ dependencies of this [`Unit`].
    dependencies_by_id: Vec<u64>,
    /// What artifacts should this unit produce.
    package_type: ArtifactsType,
}

impl Unit {
    /// Create a new [`Unit`].
    pub fn new(
        id: u64,
        package: CompilerPackage,
        storage_id: PackageId,
        dependencies: Vec<u64>,
        package_type: ArtifactsType,
    ) -> Self {
        Self {
            inner: Arc::new(UnitInner {
                id,
                package,
                storage_id,
                dependencies_by_id: dependencies,
                package_type,
            }),
        }
    }

    /// Get the unique ID of this [`Unit`].
    pub fn id(&self) -> u64 {
        self.inner.id
    }

    /// Get the root package of this [`Unit`].
    pub fn root_package(&self) -> &CompilerPackage {
        &self.inner.package
    }

    /// Get the ID's of all __direct__ dependencies of this [`Unit`].
    pub fn deps_ids(&self) -> &[u64] {
        &self.inner.dependencies_by_id
    }

    /// Get the type of produced artifacts by this [`Unit`].
    pub fn artifacts_type(&self) -> ArtifactsType {
        self.inner.package_type
    }

    /// Get a unique (in terms of the current compilation graph) name, which can be used as a directory
    /// name for storing artifacts.
    pub fn unique_name(&self) -> String {
        let id = self.inner.storage_id.storage_name();
        // Can we trim this hash?
        let id = sha256_string(id);
        let name = self.root_package().package().manifest().name();
        let version = self.root_package().package().manifest().version();
        format!("{}-{}-{}", name, version, id)
    }
}

impl PartialEq for Unit {
    fn eq(&self, other: &Self) -> bool {
        Arc::ptr_eq(&self.inner, &other.inner)
    }
}

impl Eq for Unit {}
