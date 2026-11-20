// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Build script of QuackPack: makes its own binaries (unit tests) find a bundled `libscip`.
//! The `duck` binary gets the same `rpath` from the `duck` package's build script.

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    // Bundled scip is a DLL, built by `scip-sys` (a direct dependency, so we see its `DEP_*`).
    if std::env::var("CARGO_FEATURE_BUNDLED_SCIP").is_ok() {
        let libscip_dir = std::env::var("DEP_SCIP_LIBDIR")
            .expect("`scip-sys` is not a direct dependency in Cargo.toml");
        println!("cargo:rustc-link-arg=-Wl,-rpath,{libscip_dir}");
        println!("cargo:rerun-if-changed={libscip_dir}");
    }
}
