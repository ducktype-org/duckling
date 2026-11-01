// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module for constructing and saving to the disk fingerprints of the build process.

mod build_data;
mod manager;

pub use build_data::*;
#[doc(inline)]
pub use manager::*;
