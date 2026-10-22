// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::path::PathBuf;

use url::Url;

use crate::{QuackResult, qp_internal};

pub trait ToPathBuf {
    fn to_path_buf(&self) -> QuackResult<PathBuf>;
}

impl ToPathBuf for Url {
    fn to_path_buf(&self) -> QuackResult<PathBuf> {
        self.to_file_path()
            .map_err(|_| qp_internal!("failed to convert url into a path: {self:?}"))
    }
}
