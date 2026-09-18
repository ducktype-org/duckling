use std::process::Command;

/// Simple build script for local development of duck.
fn main() {
    add_rpath_for_bundled_scip();
    let info = get_duck_version_info();
    emit_duck_version_info(info);
}

/// Adds an `rpath` to the `libscip` dll to the `duck` binary.
fn add_rpath_for_bundled_scip() {
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

struct DuckVersionInfo {
    commit_info: DuckCommitInfo,
    rustc_target: String,
    rustc_host: String,
    debug: String,
    opt_level: String,
    profile: String,
}

struct DuckCommitInfo {
    long_hash: String,
    short_hash: String,
    hash_date: String,
}

fn get_commit_info() -> DuckCommitInfo {
    let long_hash = {
        let mut command = Command::new("git");
        let output = command
            .args(["rev-parse", "HEAD"])
            .output()
            .expect("`git` failed");
        if !output.status.success() {
            panic!("`git rev-parse HEAD` failed: {output:?}");
        }
        let hash = String::from_utf8(output.stdout).expect("hash not a utf8");
        hash.trim().to_string()
    };

    let short_hash = {
        let mut command = Command::new("git");
        let output = command
            .args(["rev-parse", "--short", long_hash.as_str()])
            .output()
            .expect("`git` failed");
        if !output.status.success() {
            panic!("`git rev-parse --short {long_hash}` failed: {output:?}");
        }
        let hash = String::from_utf8(output.stdout).expect("hash not a utf8");
        hash.trim().to_string()
    };

    let hash_date = {
        let mut command = Command::new("git");
        let output = command
            .args(["show", "--no-patch", "--format=%cs", long_hash.as_str()])
            .output()
            .expect("`git` failed");
        if !output.status.success() {
            panic!("`git show --no-patch --format=%ci {long_hash}` failed: {output:?}");
        }
        let date = String::from_utf8(output.stdout).expect("date not a utf8");
        date.trim().to_string()
    };
    DuckCommitInfo {
        long_hash,
        short_hash,
        hash_date,
    }
}

fn get_duck_version_info() -> DuckVersionInfo {
    use std::env::var;
    let commit_info = get_commit_info();
    // https://doc.rust-lang.org/cargo/reference/environment-variables.html#environment-variables-cargo-sets-for-build-scripts
    let target = var("TARGET").unwrap();
    let host = var("HOST").unwrap();
    let debug = var("DEBUG").unwrap();
    let profile = var("PROFILE").unwrap();
    let opt_level = var("OPT_LEVEL").unwrap();
    DuckVersionInfo {
        commit_info,
        rustc_target: target,
        rustc_host: host,
        profile,
        debug,
        opt_level,
    }
}

/// Note: `duck::duck::version::Version::get` depends on these names.
fn emit_duck_version_info(info: DuckVersionInfo) {
    println!(
        "cargo:rustc-env=DUCK_LONG_COMMIT={}",
        info.commit_info.long_hash
    );

    println!(
        "cargo:rustc-env=DUCK_SHORT_COMMIT={}",
        info.commit_info.short_hash
    );

    println!(
        "cargo:rustc-env=DUCK_COMMIT_DATE={}",
        info.commit_info.hash_date
    );

    println!("cargo:rustc-env=DUCK_TARGET={}", info.rustc_target);

    println!("cargo:rustc-env=DUCK_HOST={}", info.rustc_host);

    println!("cargo:rustc-env=DUCK_PROFILE={}", info.profile);

    println!("cargo:rustc-env=DUCK_DEBUG={}", info.debug);

    println!("cargo:rustc-env=DUCK_OPT_LEVEL={}", info.opt_level);
}
