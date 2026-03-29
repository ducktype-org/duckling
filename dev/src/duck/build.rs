/// Simple build script for local development of duck.
///
/// Adds an `rpath` to the `libscip` dll to the `duck` binary.
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
    // We don't compile scip statically, link against local DLL.
    if russcip_bundled {
        let libscip_dir = std::env::var("DEP_SCIP_LIBDIR")
            .expect("`scip-sys` is not a direct dependency in Cargo.toml");
        println!("cargo:rustc-link-arg=-Wl,-rpath,{}", libscip_dir);
    }
}
