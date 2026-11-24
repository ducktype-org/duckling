// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Build script of QuackPack: rejects invalid combinations of the `scip` features.

fn main() {
    let russcip_bundled = std::env::var("CARGO_FEATURE_BUNDLED_SCIP").is_ok();
    let russcip_static = std::env::var("CARGO_FEATURE_STATIC_SCIP").is_ok();
    if russcip_static && russcip_bundled {
        panic!(
            "scip is bundled and built from source at the same time, but bundled will take precedence, which is not intended\n\
            hint: pass `--no-default-features` to cargo to disable bundled scip\n\
            hint: don't pass `--features=static-scip` nor `--features=bundled-all` to disable static scip"
        )
    }
    if !russcip_bundled && !russcip_static {
        panic!(
            "scip is neither bundled nor built from source, which is not intended\
            hint: don't pass `--no-default-features` to cargo to build bundled scip\n\
            hint: pass `--features=static-scip` or `--features=bundled-all` to build static scip"
        )
    }
}
