// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Various (for us mostly unneeded) metadata of the root package.

#[derive(Clone, Debug, Default)]
/// Various package metadata.
/// This is mostly useless information for us, but it may be useful for a user.
pub struct PackageMetadata {
    pub authors: Vec<String>,
    pub license: Option<String>,
    pub description: Option<String>,
}
