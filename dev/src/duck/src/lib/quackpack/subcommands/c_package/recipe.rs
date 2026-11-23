// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! `c-package.yaml`: how the bindings of a C package are generated.
//!
//! The recipe is published with the package, so every path in it is relative to the package's
//! root, and libraries are named rather than located.

use std::fs;
use std::path::{Component, Path, PathBuf};

use serde::{Deserialize, Serialize};

use super::package::write;
use crate::{QuackResult, QuackResultContext};

/// File name of the recipe, in the package's root.
pub const RECIPE_FILE: &str = "c-package.yaml";

const RECIPE_HEADER: &str = "\
# Recipe of `duck c-package`: how the bindings in `src/generated/` are made from C headers.
# Edit it and run `duck c-package regen` to update them. Paths are relative to this file.
";

#[derive(Clone, Debug, Default, Deserialize, Serialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case", deny_unknown_fields)]
/// Everything `duck c-package regen` needs to regenerate the bindings.
pub struct Recipe {
    /// Headers to translate: a path, or a name found on the include path (`SDL3/SDL.h`).
    pub headers: Vec<String>,
    /// pkg-config packages providing compiler flags and library names.
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub pkg_config: Vec<String>,
    /// Extra compiler flags, such as `-I` and `-D`.
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub cflags: Vec<String>,
    /// Extra libraries: `-l<name>`, `-L<dir>`, or a path to a `lib<name>.a`/`.so`/`.dylib`.
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub libraries: Vec<String>,
    /// Name globs a declaration has to match to be translated; empty means all.
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub include: Vec<String>,
    /// Name globs of declarations to leave out.
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub exclude: Vec<String>,
}

impl Recipe {
    /// Reads the recipe of the package rooted at `root`.
    pub fn read(root: &Path) -> QuackResult<Self> {
        let path = root.join(RECIPE_FILE);
        let text = fs::read_to_string(&path).with_context(|| {
            format!(
                "failed to read `{}`; create the package with `duck c-package new`",
                path.display()
            )
        })?;
        serde_yaml_ng::from_str(&text)
            .with_context(|| format!("`{}` is not a valid recipe", path.display()))
    }

    /// Writes the recipe into the package rooted at `root`.
    pub fn write(&self, root: &Path) -> QuackResult<()> {
        let yaml = serde_yaml_ng::to_string(self).context("failed to serialize the recipe")?;
        write(&root.join(RECIPE_FILE), &format!("{RECIPE_HEADER}{yaml}"))
    }

    /// Rewrites the paths given on the command line, relative to `cwd`, as paths relative to the
    /// package's `root`. Header names that are not files (`SDL3/SDL.h`) and `-l` flags are kept.
    pub fn relativize(&mut self, cwd: &Path, root: &Path) {
        for header in &mut self.headers {
            let path = cwd.join(&*header);
            if path.is_file() {
                *header = relative_path(&path, root);
            }
        }
        for flag in &mut self.cflags {
            if let Some(dir) = flag.strip_prefix("-I") {
                *flag = format!("-I{}", relative_path(&cwd.join(dir), root));
            }
        }
        for library in &mut self.libraries {
            if let Some(dir) = library.strip_prefix("-L") {
                *library = format!("-L{}", relative_path(&cwd.join(dir), root));
            } else if !library.starts_with('-') {
                *library = relative_path(&cwd.join(&*library), root);
            }
        }
    }
}

/// `path` relative to `base`, with `/` separators. Both are made absolute and normalized first,
/// so it also works for files that do not exist yet.
pub fn relative_path(path: &Path, base: &Path) -> String {
    let path = normalize(path);
    let base = normalize(base);
    let common = path
        .components()
        .zip(base.components())
        .take_while(|(a, b)| a == b)
        .count();
    let ups = base.components().count() - common;
    let parts = std::iter::repeat_n("..".to_owned(), ups).chain(
        path.components()
            .skip(common)
            .map(|part| part.as_os_str().to_string_lossy().into_owned()),
    );
    let relative = parts.collect::<Vec<_>>().join("/");
    if relative.is_empty() {
        ".".to_owned()
    } else {
        relative
    }
}

fn normalize(path: &Path) -> PathBuf {
    let absolute = if path.is_absolute() {
        path.to_path_buf()
    } else {
        std::env::current_dir().unwrap_or_default().join(path)
    };
    let mut result = PathBuf::new();
    for component in absolute.components() {
        match component {
            Component::CurDir => {}
            Component::ParentDir => {
                result.pop();
            }
            other => result.push(other),
        }
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn relative_paths() {
        let root = Path::new("/work/pkg");
        assert_eq!(
            relative_path(Path::new("/work/pkg/include/a.h"), root),
            "include/a.h"
        );
        assert_eq!(
            relative_path(Path::new("/work/vendor/a.h"), root),
            "../vendor/a.h"
        );
        assert_eq!(
            relative_path(Path::new("/work/pkg/./x/../b.h"), root),
            "b.h"
        );
        assert_eq!(relative_path(root, root), ".");
    }

    #[test]
    fn recipe_round_trips() {
        let dir = tempfile::tempdir().unwrap();
        let recipe = Recipe {
            headers: vec!["SDL3/SDL.h".into()],
            pkg_config: vec!["sdl3".into()],
            include: vec!["SDL_*".into()],
            ..Default::default()
        };
        recipe.write(dir.path()).unwrap();
        let text = fs::read_to_string(dir.path().join(RECIPE_FILE)).unwrap();
        assert!(text.starts_with("# Recipe of `duck c-package`"), "{text}");
        assert_eq!(Recipe::read(dir.path()).unwrap(), recipe);
    }

    #[test]
    fn command_line_paths_become_relative_to_the_package() {
        let dir = tempfile::tempdir().unwrap();
        let cwd = dir.path();
        let root = cwd.join("pkg");
        fs::create_dir_all(cwd.join("vendor")).unwrap();
        fs::write(cwd.join("vendor/foo.h"), "").unwrap();
        let mut recipe = Recipe {
            headers: vec!["vendor/foo.h".into(), "zlib.h".into()],
            cflags: vec!["-Ivendor".into(), "-DFOO".into()],
            libraries: vec!["-Lbuild".into(), "-lfoo".into(), "build/libbar.a".into()],
            ..Default::default()
        };
        recipe.relativize(cwd, &root);
        assert_eq!(recipe.headers, ["../vendor/foo.h", "zlib.h"]);
        assert_eq!(recipe.cflags, ["-I../vendor", "-DFOO"]);
        assert_eq!(
            recipe.libraries,
            ["-L../build", "-lfoo", "../build/libbar.a"]
        );
    }
}
