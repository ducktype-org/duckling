//! Checks a generated package by building and running a program against it.

use std::path::Path;
use std::process::Command;

use super::package::{GENERATED_DIR, write};
use crate::util::command_ext::CommandExt;
use crate::{DuckContext, QuackResult, QuackResultContext, qp_bail};

/// Which backends to check the package on.
pub struct Backends {
    pub native: bool,
    pub dvm: bool,
}

/// Builds a program depending on the package at `package_root` and runs its layout check on each
/// backend: it compiles every declaration, links against the library, and compares every class
/// layout with the one clang reported.
pub fn verify(
    ctx: &DuckContext,
    package_root: &Path,
    name: &str,
    layouts: &[String],
    backends: &Backends,
) -> QuackResult<()> {
    let dir = tempfile::tempdir().context("failed to create a directory for the check")?;
    let app = dir.path().join("translate_c_check");
    let package_path = package_root
        .canonicalize()
        .with_context(|| format!("failed to resolve `{}`", package_root.display()))?;
    write(
        &app.join("quackconfig.yaml"),
        &format!(
            "metadata:\n  name: translate_c_check\n  version: '1.0.0'\n\
             dependencies:\n  {name}:\n    source:\n      path: '{}'\n\
             profiles:\n  dvm:\n    dvm-bytecode: true\n",
            package_path.display()
        ),
    )?;
    write(
        &app.join("src").join("src.dk"),
        &format!(
            "import core.builtins.*;\n\
             import {name}.{GENERATED_DIR}.layout_check.*;\n\n\
             fun main() -> i64 = {{\n\
             \x20   let mismatch = c_layout_mismatch();\n\
             \x20   if (mismatch == 0i64) {{ return 0i64; }}\n\
             \x20   builtin_output_i64(mismatch);\n\
             \x20   return 1i64;\n\
             }}\n"
        ),
    )?;

    let profiles = [
        ("native", "dev", backends.native),
        ("DVM", "dvm", backends.dvm),
    ];
    for (backend, profile, enabled) in profiles {
        if !enabled {
            continue;
        }
        ctx.info(format!("checking the package on the {backend} backend"))?;
        let mut command = Command::new(std::env::current_exe().context("failed to locate duck")?);
        command
            .arg("-C")
            .arg(&app)
            .arg("run")
            .arg(format!("--profile={profile}"));
        let output = command
            .output()
            .with_context(|| format!("failed to run `{}`", command.display()))?;
        if output.status.success() {
            continue;
        }
        let stdout = String::from_utf8_lossy(&output.stdout);
        let mismatch = stdout
            .lines()
            .rev()
            .find_map(|line| line.trim().parse::<usize>().ok());
        if let Some(class) = mismatch.and_then(|index| layouts.get(index.wrapping_sub(1))) {
            qp_bail!(
                "on the {backend} backend, the size or alignment of `{class}` differs from the C \
                 one; this is a bug in `duck translate-c`"
            );
        }
        qp_bail!(
            "the generated package does not build or run on the {backend} backend; this is a bug \
             in `duck translate-c` unless the library itself is missing; the package is \
             kept for inspection.\n{}{}",
            stdout,
            String::from_utf8_lossy(&output.stderr)
        );
    }
    Ok(())
}
