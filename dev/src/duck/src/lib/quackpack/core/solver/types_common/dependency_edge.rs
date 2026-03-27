use std::collections::HashMap;

use crate::{
    QuackResult, QuackResultContext, StrId,
    quackpack::core::{
        Dependency,
        solver::types_common::ExpandedPackage,
        types_common::{InternedExpandedLocation, InternedLocation, Location},
    },
};

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
/// A struct describing a dependency of a package on some location.
pub struct DependencyEdge {
    pub parent: ExpandedPackage,
    pub dependency_loc: InternedExpandedLocation,
    pub manifest_child_name: StrId,
}

impl DependencyEdge {
    /// Given a package and a manifest entry describing its dependency,
    /// creates a [`DependencyEdge`].
    pub fn from_manifest_and_parent(
        parent: ExpandedPackage,
        manifest_dependency: &Dependency,
        location_resolver: &HashMap<InternedLocation, InternedExpandedLocation>,
    ) -> QuackResult<Self> {
        let child_loc = Location::from(manifest_dependency);
        location_resolver
            .get(&InternedLocation::new(child_loc))
            .map(|child_loc| Self {
                parent,
                dependency_loc: *child_loc,
                manifest_child_name: manifest_dependency.effective_name(),
            })
            .context_internal("Failed to expand a location")
    }
}
