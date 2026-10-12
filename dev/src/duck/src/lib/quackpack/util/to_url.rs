// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::path::{Path, PathBuf};

use url::Url;

use crate::{QuackResult, QuackResultContext, qp_err};

pub trait ToUrl {
    fn to_url(&self) -> QuackResult<Url>;
}

impl ToUrl for str {
    fn to_url(&self) -> QuackResult<Url> {
        Url::parse(self).with_context(|| format!("`{self}` is not a valid url"))
    }
}

impl ToUrl for Path {
    fn to_url(&self) -> QuackResult<Url> {
        Url::from_file_path(self)
            .map_err(|_| qp_err!("failed to turn `{}` into a url", self.display()))
    }
}

impl ToUrl for PathBuf {
    fn to_url(&self) -> QuackResult<Url> {
        self.as_path().to_url()
    }
}
