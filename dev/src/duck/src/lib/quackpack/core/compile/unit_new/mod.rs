//! [`Unit`] is supposed to be all information required to invoke a single instance of duckc.

use std::collections::{HashSet, VecDeque};
use std::convert::Infallible;
use std::env::consts::{DLL_PREFIX, DLL_SUFFIX, EXE_SUFFIX};
use std::fmt;
use std::hash::Hash;
use std::ops::ControlFlow;
use std::sync::Arc;

use super::compiler_package::CompilerPackage;
use super::duckc::multipackage_schema;
use crate::quackpack::core::identity::Identity;
use crate::quackpack::core::{AnyPackage, FeatureName};
use crate::util::hash::sha256_string;
use crate::{QuackResult, qp_bail_internal};

pub mod graph;
pub mod graph_visitor;

// Missing constants from [`std::env::consts`].
const STATIC_LIB_SUFFIX: &str = ".a";

// Duckling specific.
const DVM_SUFFIX: &str = ".dbc";

pub type UnitId = u64;

#[derive(Clone)]
/// A single action to be performed during the compilation process.
pub struct Unit {
    inner: Arc<UnitInner>,
}

impl fmt::Debug for Unit {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let inner = &*self.inner;
        f.debug_struct("Unit")
            .field("name", &inner.package.name())
            .field("version", &inner.package.version())
            .field("identity", &inner.identity)
            .field("unit_type", &inner.unit_type)
            .finish()
    }
}

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
/// What type of artifacts a given [`Unit`] produces.
pub enum UnitType {
    /// Compile to a binary
    /// Maps to the `Native` strategy
    Binary,
    /// Compile to a library (`.dll`, `.so`, `.a`, etc)
    /// Maps to the `Native` strategy
    Library,
    /// This [`Unit`] is a dependency and can produce only minimal artifacts
    /// Maps to the `Lib` compilation strategy, and we'll produce only minimal archives:
    /// they might be incomplete, but linker will take care of this (when compiling the root package
    /// with [`Binary`](Self::Binary) or [`Library`](Self::Library) types).
    Dependency,
}

struct UnitInner {
    /// Which package we're compiling.
    package: AnyPackage,
    /// Enabled features of this [`Unit`].
    enabled_features: HashSet<FeatureName>,
    /// How have we got this package.
    identity: Identity,
    /// What artifacts should this unit produce.
    unit_type: UnitType,
}

impl Unit {
    /// Create a new [`Unit`].
    pub fn new(package: CompilerPackage, identity: Identity, unit_type: UnitType) -> Self {
        let (package, enabled_features, _) = package.decompose();
        Self {
            inner: Arc::new(UnitInner {
                package,
                enabled_features,
                identity,
                unit_type,
            }),
        }
    }

    /// Get the package of this [`Unit`].
    pub fn package(&self) -> &AnyPackage {
        &self.inner.package
    }

    /// Get the enabled features of this [`Unit`].
    pub fn enabled_features(&self) -> &HashSet<FeatureName> {
        &self.inner.enabled_features
    }

    /// Get the type of produced artifacts by this [`Unit`].
    pub fn unit_type(&self) -> UnitType {
        self.inner.unit_type
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
        let name = self.package().name();
        let version = self.package().version();
        format!("{}-{}-{}", name, version, id)
    }

    /// Get a descriptive name of this [`Unit`].
    ///
    /// It's a _nice_ name, which can be displayed to the user.
    pub fn descriptive_name(&self) -> String {
        let name = self.package().name();
        let version = self.package().version();
        format!("{name} version {version}")
    }

    /// Get the filename of the output of this [`Unit`].
    pub fn output_file_name(&self) -> String {
        let name = self.package().name();
        match self.unit_type() {
            UnitType::Binary => format!("{}{}", name, EXE_SUFFIX),
            UnitType::Library => format!("{}{}{}", DLL_PREFIX, name, DLL_SUFFIX),
            UnitType::Dependency => {
                format!("{}{}", self.unique_name(), STATIC_LIB_SUFFIX)
            }
        }
    }
    /*
    /// Get a single [`multipackage_schema::Package`] for this [`Unit`].
    pub fn multipackage_schema_package(
        &self,
        graph: &UnitGraph,
    ) -> QuackResult<multipackage_schema::Package> {
        let package = self.package();
        let import_name = package.normalised_name();
        let version = package.version();
        let features = {
            let mut features = self.enabled_features().iter().copied().collect::<Vec<_>>();
            features.sort();
            features
        };
        let dependencies = {
            let mut result = vec![];
            for dep_id in graph.deps_for(self.unit_id()) {
                let unit_dep = graph.unit_for(*dep_id);
                let dep_name = unit_dep.package().name();
                let dep = package
                    .manifest()
                    .dependencies()
                    .get_by_name(dep_name)
                    .unwrap_or_else(|| {
                        panic!(
                            "unit=({},{}) has dep=({},{}), but it's not in the manifest?! `{self:?}` {graph:#?}",
                            self.unit_id(),
                            import_name,
                            dep_id,
                            dep_name
                        )
                    });
                result.push(multipackage_schema::Dependency {
                    id: unit_dep.unique_name().into(),
                    alias: dep.normalised_alias(),
                });
            }
            result
        };
        let Some(source_directory) = package.src() else {
            qp_bail_internal!(
                "asked for src directory of the global package or a script: {package:#?}"
            )
        };
        Ok(multipackage_schema::Package {
            id: self.unique_name().into(),
            import_name,
            version,
            features,
            path_to_the_src_directory: source_directory.to_path_buf(),
            dependencies,
        })
    }

    /// Accept a [`UnitVisitor`].
    ///
    /// This method should only drive the visitor through dependencies of this [`Unit`].
    ///
    /// If visitor returns `ControlFlow::Break(b)`, we short circuit to `Some(b)`.
    ///
    /// Otherwise (no breaks), we return `None`.
    pub fn accept<V: UnitVisitor + ?Sized>(
        &self,
        visitor: &mut V,
        graph: &UnitGraph,
    ) -> Option<V::Break> {
        struct VisitorAsTryVisitor<'a, U: ?Sized> {
            visitor: &'a mut U,
        }
        impl<U: UnitVisitor + ?Sized> TryUnitVisitor for VisitorAsTryVisitor<'_, U> {
            type Err = Infallible;

            type Break = U::Break;

            fn try_visit(&mut self, unit: &Unit) -> Result<ControlFlow<Self::Break>, Self::Err> {
                Ok(self.visitor.visit(unit))
            }
        }
        let result = self.try_accept(&mut VisitorAsTryVisitor { visitor }, graph);
        let Ok(result) = result;
        result
    }

    /// Accept a [`TryUnitVisitor`].
    ///
    /// This method should only drive the visitor through dependencies of this [`Unit`].
    ///
    /// If visitor returns an `Err(e)`, we short circuit to `Err(e)`
    ///
    /// If it returns `Ok(ControlFlow::Break(b))`, we short circuit to `Ok(Some(b))`.
    ///
    /// Otherwise (no errors + no breaks), we return `Ok(None)`.
    pub fn try_accept<V: TryUnitVisitor + ?Sized>(
        &self,
        visitor: &mut V,
        graph: &UnitGraph,
    ) -> Result<Option<V::Break>, V::Err> {
        let mut stack = VecDeque::from([self.unit_id()]);
        let mut visited = HashSet::new();
        while let Some(id) = stack.pop_front() {
            if visited.contains(&id) {
                continue;
            }
            visited.insert(id);
            let unit = graph.unit_for(id);
            if let ControlFlow::Break(b) = visitor.try_visit(unit)? {
                return Ok(Some(b));
            }
            stack.extend(graph.deps_for(unit.unit_id()));
        }
        Ok(None)
    }*/
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
