//! Turns a recipe into concrete compiler flags, linker arguments and DVM shared objects.

use std::path::{Path, PathBuf};
use std::process::Command;

use tracing::debug;

use super::elf::soname_of_file;
use super::recipe::CBindingsRecipe;
use crate::{QuackResult, QuackResultContext, qp_bail};

/// Directories the dynamic loader searches by default, so a library found there can be named by
/// its soname alone.
const SYSTEM_LIBRARY_DIRS: &[&str] = &[
    "/lib",
    "/lib64",
    "/usr/lib",
    "/usr/lib64",
    "/lib/x86_64-linux-gnu",
    "/usr/lib/x86_64-linux-gnu",
    "/lib/aarch64-linux-gnu",
    "/usr/lib/aarch64-linux-gnu",
    "/usr/local/lib",
    "/usr/local/lib64",
];

#[derive(Debug, Default, PartialEq, Eq)]
/// A recipe resolved on this machine.
pub struct Resolved {
    /// Flags passed to clang when reading the headers.
    pub cflags: Vec<String>,
    /// `metadata.links`: arguments for the native linker.
    pub links: Option<String>,
    /// `metadata.dvm-shared-libs`: shared objects the DVM `dlopen`s.
    pub dvm_shared_libs: Vec<String>,
    /// `(pkg-config package, version)` of every pkg-config package.
    pub versions: Vec<(String, String)>,
    /// Libraries the DVM cannot load, with the reason.
    pub dvm_warnings: Vec<String>,
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

/// Resolves `recipe`: pkg-config packages are queried, library paths made absolute against
/// `base`, and every shared library located for the DVM.
pub fn resolve(recipe: &CBindingsRecipe, base: &Path) -> QuackResult<Resolved> {
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
    libraries.extend(recipe.libraries.iter().map(|library| {
        if library.starts_with('-') {
            library.clone()
        } else {
            absolute(base, library).display().to_string()
        }
    }));
    debug!(?libraries, cflags = ?resolved.cflags, "resolved recipe");

    if !libraries.is_empty() {
        resolved.links = Some(libraries.join(" "));
    }
    let (shared, warnings) = dvm_shared_libraries(&libraries);
    resolved.dvm_shared_libs = shared;
    resolved.dvm_warnings = warnings;
    Ok(resolved)
}

fn absolute(base: &Path, path: &str) -> PathBuf {
    let path = Path::new(path);
    if path.is_absolute() {
        path.to_path_buf()
    } else {
        base.join(path)
    }
}

fn is_shared_object(path: &Path) -> bool {
    path.file_name()
        .and_then(|name| name.to_str())
        .is_some_and(|name| name.ends_with(".so") || name.contains(".so."))
}

/// The shared objects the DVM loads in place of the linker arguments `libraries`.
fn dvm_shared_libraries(libraries: &[String]) -> (Vec<String>, Vec<String>) {
    let mut search_dirs: Vec<PathBuf> = vec![];
    let mut names = vec![];
    let mut shared = vec![];
    let mut warnings = vec![];

    let mut tokens = libraries.iter();
    while let Some(token) = tokens.next() {
        if let Some(dir) = token.strip_prefix("-L") {
            let dir = if dir.is_empty() {
                tokens.next().map(String::as_str).unwrap_or_default()
            } else {
                dir
            };
            search_dirs.push(PathBuf::from(dir));
        } else if let Some(name) = token.strip_prefix("-l") {
            names.push(name.to_owned());
        } else if !token.starts_with('-') {
            let path = Path::new(token);
            if is_shared_object(path) {
                shared.push(token.clone());
            } else {
                warnings.push(format!(
                    "`{token}` is not a shared object, so the DVM cannot load it"
                ));
            }
        }
    }

    for name in names {
        match find_library(&name, &search_dirs) {
            Some(found) => shared.push(found),
            None => warnings.push(format!(
                "no shared object found for `-l{name}`, so the DVM cannot load it"
            )),
        }
    }
    shared.dedup();
    (shared, warnings)
}

/// Finds `-l<name>`: a library in a system directory is named by its soname, one found only
/// through `-L` by its full path.
fn find_library(name: &str, search_dirs: &[PathBuf]) -> Option<String> {
    let file_name = match name.strip_prefix(':') {
        Some(exact) => exact.to_owned(),
        None => format!("lib{name}.so"),
    };
    let system = SYSTEM_LIBRARY_DIRS.iter().map(PathBuf::from);
    for (dir, is_system) in search_dirs
        .iter()
        .cloned()
        .map(|dir| (dir, false))
        .chain(system.map(|dir| (dir, true)))
    {
        let candidate = dir.join(&file_name);
        if !candidate.exists() {
            continue;
        }
        let soname = soname_of_file(&candidate);
        return Some(match (soname, is_system) {
            (Some(soname), true) => soname,
            (Some(soname), false) if dir.join(&soname).exists() => {
                dir.join(soname).display().to_string()
            }
            _ => candidate.display().to_string(),
        });
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn static_artifacts_are_not_loadable_by_the_dvm() {
        let (shared, warnings) =
            dvm_shared_libraries(&["/tmp/foo.o".into(), "/tmp/libbar.so.1".into()]);
        assert_eq!(shared, vec!["/tmp/libbar.so.1".to_owned()]);
        assert_eq!(warnings.len(), 1);
    }

    #[test]
    fn missing_library_is_reported() {
        let (shared, warnings) =
            dvm_shared_libraries(&["-lthis_library_does_not_exist_anywhere".into()]);
        assert!(shared.is_empty());
        assert_eq!(warnings.len(), 1);
    }

    #[test]
    #[cfg(target_os = "linux")]
    fn system_library_is_named_by_its_soname() {
        let (shared, _) = dvm_shared_libraries(&["-lm".into()]);
        if let Some(found) = shared.first() {
            assert!(found.starts_with("libm.so"), "{found}");
        }
    }

    #[test]
    fn relative_libraries_resolve_against_the_base() {
        let recipe = CBindingsRecipe {
            headers: vec!["foo.h".into()],
            libraries: vec!["build/foo.o".into(), "-lfoo".into()],
            ..Default::default()
        };
        let resolved = resolve(&recipe, Path::new("/work")).unwrap();
        assert_eq!(resolved.links.as_deref(), Some("/work/build/foo.o -lfoo"));
    }
}
