// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! High level type of compilation, supported by both duck and duckc.
//!
//! Right now we only support compiling the root package, but in future there'll be more options.
use std::fmt;

#[non_exhaustive]
#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
/// High level type of compilation, supported by both duck and duckc.
///
/// Right now we only support compiling the root package, but in future there'll be more options.
pub enum CompilationType {
    OnlyRootPackage,
    StandaloneScript,
}

impl fmt::Display for CompilationType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::OnlyRootPackage => write!(f, "only root package"),
            Self::StandaloneScript => write!(f, "standalone script"),
        }
    }
}
