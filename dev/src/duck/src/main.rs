// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/// The entry point of the duck binary: the common duck tool entry point, driving [`duck::run`].
fn main() {
    duck_core::entry::main(duck::run)
}
