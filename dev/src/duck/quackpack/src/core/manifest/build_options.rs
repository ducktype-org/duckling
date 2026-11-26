// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Various options for controlling the packages' compilation.

use std::path::PathBuf;

use crate::StrId;

#[derive(Clone, Debug)]
/// Options for controlling the build process.
pub struct BuildOptions {
    /// Human-friendly names of the external libraries to link against (`sqlite3`, not
    /// `libsqlite3.a`).
    pub external_libs: Vec<StrId>,
    /// Directories searched for [`BuildOptions::external_libs`] before the system ones.
    /// They are relative to the package's root.
    pub external_lib_paths: Vec<PathBuf>,
    /// Whether the package is compiled with the standard library.
    /// A package without it can only depend on packages without it.
    pub use_std: bool,
    /// Whether the package can only be compiled as a dependency of another package.
    /// Such a package has no dependencies.
    pub always_dep: bool,
}

impl Default for BuildOptions {
    fn default() -> Self {
        Self {
            external_libs: Vec::new(),
            external_lib_paths: Vec::new(),
            use_std: true,
            always_dep: false,
        }
    }
}
