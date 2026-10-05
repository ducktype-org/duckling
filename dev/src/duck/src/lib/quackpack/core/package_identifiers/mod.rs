// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! A module containing different ways of identifying packages.
//! All the intricacies are described in the `readme.md`.

pub mod full_identity;
pub mod identity;
mod package_id;

pub use package_id::*;
