// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Interned values, cheap to copy and compare: [`StrId`](str_id::StrId) and
//! [`InternedUrl`](interned_url::InternedUrl).
pub mod interned_url;
pub mod str_id;
