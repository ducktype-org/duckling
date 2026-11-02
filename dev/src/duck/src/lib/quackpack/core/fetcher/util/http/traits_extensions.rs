// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use serde::Deserialize;

use super::Response;

pub trait ResponseExt {
    fn deserialize_json<T>(&self) -> serde_json::Result<T>
    where
        T: for<'de> Deserialize<'de>;
}

impl ResponseExt for Response {
    fn deserialize_json<T>(&self) -> serde_json::Result<T>
    where
        T: for<'de> Deserialize<'de>,
    {
        serde_json::from_slice(self.body())
    }
}
