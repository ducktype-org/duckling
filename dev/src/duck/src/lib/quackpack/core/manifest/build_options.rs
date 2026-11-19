// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Various options for controlling the packages' compilation.

use std::path::PathBuf;

use crate::StrId;
use crate::quackpack::schemas::manifest as schema;

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
/// How an external library is linked.
pub enum LinkKind {
    /// Against the shared library (`libfoo.so`, `libfoo.dylib`); the system's copy is used at
    /// runtime.
    #[default]
    Dynamic,
    /// The static archive (`libfoo.a`) is copied into the produced binary.
    Static,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
/// An external library to link against.
pub struct ExternalLib {
    /// Human-friendly name of the library (`sqlite3`, not `libsqlite3.a`).
    pub name: StrId,
    /// How the library is linked.
    pub kind: LinkKind,
}

impl ExternalLib {
    /// A library linked dynamically.
    pub fn dynamic(name: impl Into<StrId>) -> Self {
        Self {
            name: name.into(),
            kind: LinkKind::Dynamic,
        }
    }
}

impl From<schema::LinkKind> for LinkKind {
    fn from(value: schema::LinkKind) -> Self {
        match value {
            schema::LinkKind::Dynamic => LinkKind::Dynamic,
            schema::LinkKind::Static => LinkKind::Static,
        }
    }
}

impl From<&schema::ExternalLib> for ExternalLib {
    fn from(value: &schema::ExternalLib) -> Self {
        Self {
            name: value.name().into(),
            kind: value.kind().into(),
        }
    }
}

impl From<ExternalLib> for schema::ExternalLib {
    /// A dynamic library is written as its bare name, a static one as a table.
    fn from(value: ExternalLib) -> Self {
        match value.kind {
            LinkKind::Dynamic => schema::ExternalLib::Name(value.name.to_string()),
            LinkKind::Static => schema::ExternalLib::Detailed(schema::DetailedExternalLib {
                name: value.name.to_string(),
                kind: Some(schema::LinkKind::Static),
            }),
        }
    }
}

#[derive(Clone, Debug, Default)]
/// Options for controlling the build process.
pub struct BuildOptions {
    /// The external libraries to link against.
    pub external_libs: Vec<ExternalLib>,
    /// Directories searched for [`BuildOptions::external_libs`] before the system ones.
    /// They are relative to the package's root.
    pub external_lib_paths: Vec<PathBuf>,
}
