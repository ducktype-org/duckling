fn main() {
    let libscip_dir = std::env::var("DEP_SCIP_LIBDIR").unwrap();
    println!("cargo:rustc-link-arg=-Wl,-rpath,{}", libscip_dir);
}
