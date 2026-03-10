fn main() {
    let libscip_dir = std::env::var("DEP_SCIP_LIBDIR")
        .expect("`scip-sys` is not a direct dependency in Cargo.toml");
    println!("cargo:rustc-link-arg=-Wl,-rpath,{}", libscip_dir);
}
