// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Turns a recipe into compiler flags for reading the headers and into the library names written
//! to the package's manifest.
//!
//! Nothing machine-specific goes into the manifest: libraries are written as bare names and
//! duck finds their files when the package is built.

use std::path::Path;
use std::process::Command;

use tracing::debug;

use super::recipe::{Recipe, relative_path};
use crate::quackpack::core::compile::unit_runner::external_libs::DEFAULT_EXTERNAL_LIB_PATHS;
use crate::{QuackResult, QuackResultContext, qp_bail};

#[derive(Debug, Default, PartialEq, Eq)]
/// A recipe resolved on this machine.
pub struct Resolved {
    /// Flags passed to clang when reading the headers.
    pub cflags: Vec<String>,
    /// `metadata.external-libs`: bare library names.
    pub external_libs: Vec<String>,
    /// `metadata.external-lib-paths`: directories relative to the package's root, or absolute
    /// directories that duck does not search by default.
    pub external_lib_paths: Vec<String>,
    /// `(pkg-config package, version)` of every pkg-config package.
    pub versions: Vec<(String, String)>,
    /// Linker flags that were left out, with the reason.
    pub warnings: Vec<String>,
}

/// Runs `pkg-config` with `args`, returning its whitespace-separated output.
fn pkg_config(args: &[&str], packages: &[String]) -> QuackResult<Vec<String>> {
    let mut command = Command::new("pkg-config");
    command.args(args).args(packages);
    let output = command
        .output()
        .context("failed to run `pkg-config`; is it installed?")?;
    if !output.status.success() {
        qp_bail!(
            "`pkg-config {} {}` failed: {}",
            args.join(" "),
            packages.join(" "),
            String::from_utf8_lossy(&output.stderr).trim()
        );
    }
    Ok(String::from_utf8_lossy(&output.stdout)
        .split_whitespace()
        .map(str::to_owned)
        .collect())
}

/// Resolves `recipe` for the package rooted at `root`.
pub fn resolve(recipe: &Recipe, root: &Path) -> QuackResult<Resolved> {
    let mut resolved = Resolved::default();
    let mut libraries = vec![];
    if !recipe.pkg_config.is_empty() {
        resolved.cflags = pkg_config(&["--cflags"], &recipe.pkg_config)?;
        libraries = pkg_config(&["--libs"], &recipe.pkg_config)?;
        for package in &recipe.pkg_config {
            let version = pkg_config(&["--modversion"], std::slice::from_ref(package))?.join(" ");
            resolved.versions.push((package.clone(), version));
        }
    }
    resolved.cflags.extend(recipe.cflags.iter().cloned());
    add_platform_cflags(&mut resolved.cflags);
    libraries.extend(recipe.libraries.iter().cloned());
    for library in &libraries {
        add_library(&mut resolved, library, root)?;
    }
    debug!(?resolved, "resolved recipe");
    Ok(resolved)
}

/// Adds one linker flag or library path to `resolved`.
fn add_library(resolved: &mut Resolved, library: &str, root: &Path) -> QuackResult<()> {
    if let Some(dir) = library.strip_prefix("-L") {
        add_lib_path(resolved, dir, root);
    } else if let Some(name) = library.strip_prefix("-l") {
        if name.starts_with(':') {
            resolved.warnings.push(format!(
                "`{library}` names an exact file, which `external-libs` cannot express yet (#4140); it was left out"
            ));
        } else {
            push_unique(&mut resolved.external_libs, name);
        }
    } else if library.starts_with('-') {
        resolved.warnings.push(format!(
            "the linker flag `{library}` has no `external-libs` equivalent; it was left out"
        ));
    } else {
        let path = Path::new(library);
        let file_name = path
            .file_name()
            .and_then(|name| name.to_str())
            .unwrap_or_default();
        let Some(name) = library_name(file_name) else {
            qp_bail!(
                "`{library}` is not a library; `duck c-package` links `lib<name>.a`, `.so` or \
                 `.dylib` files, so wrap object files in an archive with `ar`"
            );
        };
        if let Some(dir) = path.parent().and_then(|dir| dir.to_str()) {
            add_lib_path(resolved, if dir.is_empty() { "." } else { dir }, root);
        }
        push_unique(&mut resolved.external_libs, name);
    }
    Ok(())
}

/// Adds a library directory, as written in the recipe (relative to `root`) or by pkg-config
/// (absolute). Directories duck already searches are left out.
fn add_lib_path(resolved: &mut Resolved, dir: &str, root: &Path) {
    let path = Path::new(dir);
    if !path.is_absolute() {
        push_unique(&mut resolved.external_lib_paths, dir);
        return;
    }
    if DEFAULT_EXTERNAL_LIB_PATHS
        .iter()
        .any(|default| Path::new(default) == path)
    {
        return;
    }
    if path.starts_with(root) {
        push_unique(&mut resolved.external_lib_paths, &relative_path(path, root));
        return;
    }
    resolved.warnings.push(format!(
        "the library directory `{dir}` is specific to this machine; the package only builds \
         elsewhere if the library is installed in a standard location"
    ));
    push_unique(&mut resolved.external_lib_paths, dir);
}

/// `foo` for `libfoo.a`, `libfoo.so`, `libfoo.so.1.2` and `libfoo.dylib`.
fn library_name(file_name: &str) -> Option<&str> {
    let stem = file_name.strip_prefix("lib")?;
    let name = stem
        .strip_suffix(".a")
        .or_else(|| stem.strip_suffix(".dylib"))
        .or_else(|| stem.split_once(".so").map(|(name, _)| name))?;
    (!name.is_empty()).then_some(name)
}

fn push_unique(list: &mut Vec<String>, value: &str) {
    if !list.iter().any(|existing| existing == value) {
        list.push(value.to_owned());
    }
}

/// libclang does not know where the macOS SDK is, so its libc headers are not found without an
/// explicit sysroot.
#[cfg(target_os = "macos")]
fn add_platform_cflags(cflags: &mut Vec<String>) {
    if cflags.iter().any(|flag| flag.starts_with("-isysroot")) {
        return;
    }
    let sdk = Command::new("xcrun")
        .arg("--show-sdk-path")
        .output()
        .ok()
        .filter(|output| output.status.success())
        .map(|output| String::from_utf8_lossy(&output.stdout).trim().to_owned());
    if let Some(sdk) = sdk.filter(|sdk| !sdk.is_empty()) {
        cflags.push("-isysroot".to_owned());
        cflags.push(sdk);
    }
}

/// Reads the headers for the target the system C compiler, which links Duckling programs, builds
/// for: libclang's own default is a glibc target even on musl systems.
#[cfg(not(target_os = "macos"))]
fn add_platform_cflags(cflags: &mut Vec<String>) {
    let has_target = cflags
        .iter()
        .any(|flag| flag == "-target" || flag.starts_with("--target="));
    if has_target {
        return;
    }
    let triple = ["gcc", "cc"].iter().find_map(|compiler| {
        Command::new(compiler)
            .arg("-dumpmachine")
            .output()
            .ok()
            .filter(|output| output.status.success())
            .map(|output| String::from_utf8_lossy(&output.stdout).trim().to_owned())
            .filter(|triple| !triple.is_empty())
    });
    if let Some(triple) = triple {
        cflags.push(format!("--target={triple}"));
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn resolve_libraries(libraries: &[&str]) -> Resolved {
        let recipe = Recipe {
            headers: vec!["foo.h".into()],
            libraries: libraries
                .iter()
                .map(|&library| library.to_owned())
                .collect(),
            ..Default::default()
        };
        resolve(&recipe, Path::new("/work/pkg")).unwrap()
    }

    #[test]
    fn library_names() {
        assert_eq!(library_name("libfoo.a"), Some("foo"));
        assert_eq!(library_name("libfoo.so"), Some("foo"));
        assert_eq!(library_name("libfoo.so.1.2"), Some("foo"));
        assert_eq!(library_name("libfoo.dylib"), Some("foo"));
        assert_eq!(library_name("foo.o"), None);
        assert_eq!(library_name("lib.a"), None);
    }

    #[test]
    fn flags_and_paths_become_names_and_relative_dirs() {
        let resolved = resolve_libraries(&["-Lbuild", "-lfoo", "vendor/libbar.a", "-lfoo"]);
        assert_eq!(resolved.external_libs, ["foo", "bar"]);
        assert_eq!(resolved.external_lib_paths, ["build", "vendor"]);
        assert!(resolved.warnings.is_empty());
    }

    #[test]
    fn standard_and_package_dirs_are_not_machine_specific() {
        let resolved = resolve_libraries(&["-L/usr/lib", "-L/work/pkg/libs", "-lz"]);
        assert_eq!(resolved.external_lib_paths, ["libs"]);
        assert!(resolved.warnings.is_empty());
    }

    #[test]
    fn other_absolute_dirs_and_flags_are_reported() {
        let resolved = resolve_libraries(&["-L/opt/foo/lib", "-pthread", "-l:libz.so.1"]);
        assert_eq!(resolved.external_lib_paths, ["/opt/foo/lib"]);
        assert!(resolved.external_libs.is_empty());
        assert_eq!(resolved.warnings.len(), 3, "{:?}", resolved.warnings);
    }

    #[test]
    fn an_explicit_target_is_kept() {
        let recipe = Recipe {
            headers: vec!["foo.h".into()],
            cflags: vec!["--target=aarch64-linux-gnu".into()],
            ..Default::default()
        };
        let resolved = resolve(&recipe, Path::new("/work/pkg")).unwrap();
        let targets: Vec<_> = resolved
            .cflags
            .iter()
            .filter(|flag| flag.starts_with("--target="))
            .collect();
        assert_eq!(targets, ["--target=aarch64-linux-gnu"]);
    }

    #[test]
    fn object_files_are_rejected() {
        let recipe = Recipe {
            headers: vec!["foo.h".into()],
            libraries: vec!["build/foo.o".into()],
            ..Default::default()
        };
        assert!(resolve(&recipe, Path::new("/work/pkg")).is_err());
    }
}
