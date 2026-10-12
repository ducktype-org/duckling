// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::path::Path;

use crate::quackpack::core::PackageId;
use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
use crate::quackpack::util::to_url::ToUrl;

pub mod cycling;
pub mod tree;
pub mod unreachable_deps;

pub fn mock_local_origin(root: &Path, name: &str) -> FullOrigin {
    FullOrigin::for_local(&root.join(name)).unwrap()
}

pub fn mock_local_identity(root: &Path, name: &str) -> FullIdentity {
    FullIdentity::new(name.into(), mock_local_origin(root, name))
}

pub fn mock_registry_identity(name: &str) -> FullIdentity {
    FullIdentity::new(
        name.into(),
        FullOrigin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap()),
    )
}

pub fn mock_local_pkg(root: &Path, name: &str) -> PackageId {
    PackageId::new(mock_local_identity(root, name), 1.into())
}

pub fn mock_registry_pkg(name: &str) -> PackageId {
    PackageId::new(mock_registry_identity(name), 1.into())
}
