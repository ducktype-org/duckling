use std::{collections::HashSet, path::PathBuf};

use crate::{
    QuackResult,
    quackpack::core::{
        FeatureName, Manifest, Source,
        gathering::fetch_types::{FetchRequest, PinnedRequest, UnpinnedRequest},
        types_common::{InternedLocation, Location, Package},
    },
};

pub struct PackageData {
    pub manifest: Manifest,
    pub requested_features: HashSet<FeatureName>,
    pub local_root: Option<PathBuf>,
}

impl PackageData {
    fn dep_requests(&self) -> QuackResult<Vec<FetchRequest>> {
        let mut result = vec![];
        for dependency in self.manifest.dependencies().all_dependencies().values() {
            if !dependency.is_enabled_for(self.requested_features.iter().copied()) {
                continue;
            }
            let location = InternedLocation::new(Location::try_from(dependency)?);
            let features: HashSet<FeatureName> = HashSet::from_iter(
                dependency
                    .enabled_features(Vec::from_iter(self.requested_features.iter().copied())),
            );
            let local_root =
                if let Source::Local(local_source) = dependency.desc().source().as_ref() {
                    Some(PathBuf::from(local_source.absolute()))
                } else {
                    None
                };
            if dependency.is_pinned() {
                result.push(FetchRequest::Pinned(PinnedRequest {
                    package: Package {
                        location,
                        version: dependency.desc().versions().first().copied(),
                    },
                    features,
                    local_root,
                }));
            } else {
                result.push(FetchRequest::Unpinned(UnpinnedRequest {
                    location,
                    versions: dependency.desc().versions().to_vec(),
                    features,
                    local_root,
                }))
            }
        }
        Ok(result)
    }
}
