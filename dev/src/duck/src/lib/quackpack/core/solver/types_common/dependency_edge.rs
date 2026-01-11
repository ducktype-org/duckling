use std::collections::HashMap;

use crate::quackpack::core::{
    Dependency, Source,
    solver::types_common::{ExpandedLocation, ExpandedPackage},
    types_common::{
        Location,
        not_expanded::{LocGit, LocLocal, LocRegistry},
    },
};

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct DependencyEdge {
    pub parent: ExpandedPackage,
    pub dependency_loc: ExpandedLocation,
}

impl DependencyEdge {
    pub fn from_manifest_and_parent(
        parent: ExpandedPackage,
        manifest_dependency: &Dependency,
        location_resolver: &HashMap<Location, ExpandedLocation>,
    ) -> Option<Self> {
        let child_loc = match &manifest_dependency.desc().source() {
            Source::Registry(registry) => Location::Registry(LocRegistry {
                url: registry.url(),
                real_name: manifest_dependency.real_name(),
            }),
            Source::Local(local) => Location::Local(LocLocal {
                path: local.entry_in_manifest(),
            }),
            Source::Git(git) => Location::Git(LocGit {
                url: git.url(),
                rev: git.rev(),
                commit: git.commit(),
            }),
        };
        if let Some(child_loc) = location_resolver.get(&child_loc) {
            Some(Self {
                parent,
                dependency_loc: child_loc.clone(),
            })
        } else {
            None
        }
    }
}
