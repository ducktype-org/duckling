// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Various options for controlling the packages' compilation.

use crate::StrId;

#[derive(Clone, Debug, Default)]
/// Options for controlling the build process.
pub struct BuildOptions {
    /// Link against the specified library.
    pub links: Option<StrId>,
}
