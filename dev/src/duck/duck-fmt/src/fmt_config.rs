// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module for finding the location of the `duckfmt` configuration file from package's root.

use std::path::{Path, PathBuf};

/// Name of the `duckfmt` configuration file.
const FMT_CONFIG_FILE: &str = "fmt.json";

/// Get the `duckfmt` config file for formatting a package.
/// Currently only looks at package root for it (but might also look at duck home).
pub fn package_fmt_config_path(root: &Path) -> PathBuf {
    root.join(FMT_CONFIG_FILE)
}
