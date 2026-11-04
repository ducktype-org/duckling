// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/// The main entry point to the duck binary.
///
/// It's a thin wrapper around the [`main`](duck::main) function from the duck-library.
///
/// For a reason, why is that, consult the [`readme.md`] in the top-level duck directory.
fn main() {
    duck::main()
}
