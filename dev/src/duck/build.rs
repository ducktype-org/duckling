/// Simple build script for local development of duck.
///
/// Adds an `rpath` to the `libscip` dll to the `duck` binary.
fn main() {
    let libscip_dir = std::env::var("DEP_SCIP_LIBDIR")
        .expect("`scip-sys` is not a direct dependency in Cargo.toml");
    println!("cargo:rustc-link-arg=-Wl,-rpath,{}", libscip_dir);
}
