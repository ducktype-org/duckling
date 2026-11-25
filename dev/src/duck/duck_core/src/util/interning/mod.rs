// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Interned values, cheap to copy and compare: [`StrId`](str_id::StrId) and
//! [`InternedUrl`](interned_url::InternedUrl).
mod interned_url;
mod str_id;

pub use interned_url::InternedUrl;
pub use str_id::{QpJoin, StrId};
