// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Various options for controlling the packages' compilation.

use std::path::PathBuf;

use crate::StrId;

#[derive(Clone, Debug, Default)]
/// Options for controlling the build process.
pub struct BuildOptions {
    /// Names of the external libraries to link against (`sqlite3`, not `libsqlite3.a`).
    pub external_libs: Vec<StrId>,
    /// Directories searched for [`BuildOptions::external_libs`] before the system ones.
    /// Kept as written in the manifest: relative paths are relative to the package's root.
    pub external_lib_paths: Vec<PathBuf>,
}
