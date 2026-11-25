// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! The `build-metadata` section of a manifest: how a package takes part in a build.

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
/// How a package takes part in a build.
pub struct BuildMetadata {
    /// Whether the package is compiled with the standard library.
    /// A package without it can only depend on packages without it.
    pub use_std: bool,
    /// Whether the package can only be compiled as a dependency of another package.
    /// Such a package has no dependencies.
    pub always_dep: bool,
}

impl Default for BuildMetadata {
    fn default() -> Self {
        Self {
            use_std: true,
            always_dep: false,
        }
    }
}
