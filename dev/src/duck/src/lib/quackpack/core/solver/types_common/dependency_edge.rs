use std::collections::HashMap;

use crate::quackpack::core::{
    Dependency, Source,
    solver::types_common::ExpandedPackage,
    types_common::{
        InternedExpandedLocation, InternedLocation, Location,
        not_expanded::{LocGit, LocLocal, LocRegistry},
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
    ) -> Option<Self> {
        let child_loc = match &manifest_dependency.desc().source().inner {
            Source::Registry(registry) => Location::Registry(LocRegistry {
                url: registry.url().clone(),
                real_name: manifest_dependency.real_name(),
            }),
            Source::Local(local) => Location::Local(LocLocal {
                path: local.entry_in_manifest(),
            }),
            Source::Git(git) => Location::Git(LocGit {
                url: git.url().clone(),
                branch_or_tag: git.branch_or_tag(),
                rev: git.rev(),
            }),
        };
        location_resolver
            .get(&InternedLocation::new(child_loc))
            .map(|child_loc| Self {
                parent,
                dependency_loc: *child_loc,
            })
    }
}
