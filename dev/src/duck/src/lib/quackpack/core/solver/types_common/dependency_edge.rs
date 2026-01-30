use std::collections::HashMap;

use crate::{
    QuackResult, QuackResultContext,
    quackpack::core::{
        Dependency,
        solver::types_common::ExpandedPackage,
        types_common::{InternedExpandedLocation, InternedLocation, Location},
    },
};

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct DependencyEdge {
    pub parent: ExpandedPackage,
    pub dependency_loc: InternedExpandedLocation,
}

impl DependencyEdge {
    pub fn from_manifest_and_parent(
        parent: ExpandedPackage,
        manifest_dependency: &Dependency,
        location_resolver: &HashMap<InternedLocation, InternedExpandedLocation>,
    ) -> QuackResult<Self> {
        let child_loc = Location::try_from(manifest_dependency)?;
        location_resolver
            .get(&InternedLocation::new(child_loc))
            .map(|child_loc| Self {
                parent,
                dependency_loc: *child_loc,
            })
            .context_internal("Failed to expand a location")
    }
}
