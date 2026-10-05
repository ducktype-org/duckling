// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use url::Url;

pub trait IsLocalFile {
    fn is_local_file(&self) -> bool;
}

impl IsLocalFile for Url {
    fn is_local_file(&self) -> bool {
        self.scheme() == "file"
    }
}
