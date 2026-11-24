// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Module for finding the location of the `duckfmt` configuration file from [`AnyPackage`].

use std::path::PathBuf;

use duck::quackpack::core::AnyPackage;
use duck::{QuackResult, qp_bail_internal};

/// Name of the `duckfmt` configuration file.
const FMT_CONFIG_FILE: &str = "fmt.json";

pub trait AnyPackageFmtExt {
    /// Return the location to find the `duckfmt` configuration file.
    /// Root of the package for packages, undecided for scripts.
    fn fmt_config_path(&self) -> QuackResult<PathBuf>;
}

impl AnyPackageFmtExt for AnyPackage {
    fn fmt_config_path(&self) -> QuackResult<PathBuf> {
        if self.is_package() {
            Ok(self.root().join(FMT_CONFIG_FILE))
        } else {
            qp_bail_internal!("formatting non-package targets is currently unsupported")
        }
    }
}
