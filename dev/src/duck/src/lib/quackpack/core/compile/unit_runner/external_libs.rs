//! Various functionalities for interacting with external libraries specified in the manifest.

use std::collections::HashMap;
use std::convert::Infallible;
use std::ops::ControlFlow;
use std::path::PathBuf;

use tracing::instrument;

use crate::quackpack::core::compile::unit::Unit;
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::quackpack::core::compile::unit::unit_visitor::UnitVisitor;
use crate::{QuackResult, StrId, qp_bail};

#[derive(Debug)]
/// Marker struct indicating that we have found an external library.
///
/// It may not make sense to have explicit [`links`](ExternalLibrariesFound::links) field, but
/// otherwise it's hard for the caller to get the links field, since the [`Unit`] only stores an
/// [`Option`].
///
/// This struct has a logic invariant that
/// ```rust,ignore (illustrative)
/// self.unit.root_package().package().manifest().build_options().links == Some(self.links)
/// ```
/// but allows caller to get `links` without going through an [`Option`].
pub struct ExternalLibrariesFound {
    /// [`Unit`] which links against the library.
    pub unit: Unit,
    /// The said external library.
    pub links: StrId,
}

#[instrument(skip_all)]
/// Find a [`Unit`] in the subtree rooted at `unit` that links against an external library but
/// declares no `dvm-shared-libs`, so the DVM has nothing to load in its place.
///
/// In case there are multiple such [`Unit`]s, it's not guaranteed which one is returned.
pub fn find_links_without_dvm_shared_libs(
    unit: &Unit,
    graph: &UnitGraph,
) -> Option<ExternalLibrariesFound> {
    struct ExternalLibsVisitor;

    impl UnitVisitor for ExternalLibsVisitor {
        type Break = ExternalLibrariesFound;

        fn visit(&mut self, unit: &Unit) -> ControlFlow<Self::Break> {
            let build_options = unit.package().manifest().build_options();
            match build_options.links {
                Some(links) if build_options.dvm_shared_libs.is_empty() => {
                    ControlFlow::Break(ExternalLibrariesFound {
                        unit: unit.clone(),
                        links,
                    })
                }
                _ => ControlFlow::Continue(()),
            }
        }
    }
    unit.accept(&mut ExternalLibsVisitor {}, graph)
}

#[instrument(skip_all)]
/// Validate external libraries against which we are linking.
/// Right now this checks for duplicates of the same library.
pub fn validate_external_libraries(unit: &Unit, graph: &UnitGraph) -> QuackResult<()> {
    #[derive(Default)]
    /// Map external lib -> Unit linking with it.
    struct DuplicateLibsVisitor(HashMap<StrId, Unit>);

    impl UnitVisitor for DuplicateLibsVisitor {
        /// (Dup A, Dup B, What)
        type Break = (Unit, Unit, StrId);

        fn visit(&mut self, unit: &Unit) -> ControlFlow<Self::Break> {
            let links = unit
                .package()
                .manifest()
                .build_options()
                .links
                .as_ref()
                .copied();
            // No external libs.
            let Some(links) = links else {
                return ControlFlow::Continue(());
            };
            let maybe_previous = self.0.insert(links, unit.clone());
            // Not a duplicate.
            let Some(previous) = maybe_previous else {
                return ControlFlow::Continue(());
            };
            ControlFlow::Break((previous.clone(), unit.clone(), links))
        }
    }
    let Some((dup_a, dup_b, links)) = unit.accept(&mut DuplicateLibsVisitor::default(), graph)
    else {
        return Ok(());
    };
    qp_bail!(
        "two different packages {} and {} both link against external library `{links}`",
        dup_a.descriptive_name(),
        dup_b.descriptive_name()
    )
}

#[instrument(skip_all)]
/// Gather all external libraries of the subgraph rooted ad `unit`.
pub fn gather_external_libraries(unit: &Unit, graph: &UnitGraph) -> Vec<StrId> {
    #[derive(Default)]
    struct ExternalLibsCollector(Vec<StrId>);

    impl UnitVisitor for ExternalLibsCollector {
        type Break = Infallible;

        fn visit(&mut self, unit: &Unit) -> ControlFlow<Self::Break> {
            let links = unit
                .package()
                .manifest()
                .build_options()
                .links
                .as_ref()
                .copied();
            if let Some(links) = links {
                self.0.push(links);
            };
            ControlFlow::Continue(())
        }
    }
    let mut visitor = ExternalLibsCollector::default();
    unit.accept(&mut visitor, graph);
    visitor.0
}

#[instrument(skip_all)]
/// Gather the `dvm-shared-libs` of the subgraph rooted at `unit`.
///
/// A bare file name is kept as is, so the DVM looks it up on the system search path; any other
/// relative path is resolved against the root directory of the package declaring it.
pub fn gather_dvm_shared_libraries(unit: &Unit, graph: &UnitGraph) -> Vec<PathBuf> {
    #[derive(Default)]
    struct DvmSharedLibsCollector(Vec<PathBuf>);

    impl UnitVisitor for DvmSharedLibsCollector {
        type Break = Infallible;

        fn visit(&mut self, unit: &Unit) -> ControlFlow<Self::Break> {
            let package_root = unit.package().root();
            for lib in &unit.package().manifest().build_options().dvm_shared_libs {
                let is_bare_name = lib.parent().is_none_or(|p| p.as_os_str().is_empty());
                self.0.push(if is_bare_name {
                    lib.clone()
                } else {
                    package_root.join(lib)
                });
            }
            ControlFlow::Continue(())
        }
    }
    let mut visitor = DvmSharedLibsCollector::default();
    unit.accept(&mut visitor, graph);
    visitor.0
}
