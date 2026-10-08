// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Core quackpack's modules.
pub mod compile;
pub mod editable_manifest;
pub mod fetcher;
pub mod lints;
mod manifest;
mod package;
mod package_identifiers;
mod package_loader;
pub mod run;
pub mod solver;
pub mod storage;
pub mod valid_package_name;
mod version;

pub use manifest::*;
pub use package::*;
pub use package_identifiers::*;
pub use package_loader::*;
pub use version::Version;
