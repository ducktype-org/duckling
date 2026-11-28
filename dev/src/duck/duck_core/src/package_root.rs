// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Locating the root of a Duckling package, without parsing its manifest.
//!
//! Ecosystem tools (e.g. a formatter) only need to know where a package lives. Reading the
//! manifest is QuackPack's job.
use std::fmt::Display;
use std::path::{Path, PathBuf};

use tracing::{debug, trace};

/// File name of a package manifest. The directory holding it is the root of the package.
pub const MANIFEST_NAME: &str = "quackconfig.yaml";

#[derive(Debug)]
/// Error signifying that no package was found.
pub struct PackageNotFound {
    start_path: PathBuf,
    end_path: PathBuf,
}

impl PackageNotFound {
    /// Create a new [`PackageNotFound`], describing failure to find a package between `start` and `end`.
    pub fn new(start: &Path, end: &Path) -> Self {
        Self {
            start_path: start.to_path_buf(),
            end_path: end.to_path_buf(),
        }
    }
}

impl std::error::Error for PackageNotFound {}

impl Display for PackageNotFound {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "no manifest has been found from the `{}` to the `{}`",
            self.start_path.display(),
            self.end_path.display()
        )
    }
}

/// Find the root of the package containing `start`.
///
/// Walks up `start` and its ancestors, and returns the first directory holding a
/// [`MANIFEST_NAME`] file, or `None` when there is none. `start` is used as given: it is not
/// made absolute, so pass an absolute path to search up to the filesystem root.
pub fn find_package_root(start: &Path) -> Result<PathBuf, PackageNotFound> {
    start
        .ancestors()
        .find_map(|dir| {
            let manifest = dir.join(MANIFEST_NAME);
            trace!("checking for a manifest at `{}`", manifest.display());
            manifest.is_file().then(|| {
                debug!("found a manifest at `{}`", manifest.display());
                dir.to_path_buf()
            })
        })
        .ok_or_else(|| PackageNotFound::new(start, start.ancestors().last().unwrap_or(start)))
}

#[cfg(test)]
mod tests {
    use tempfile::tempdir;

    use super::*;
    use crate::util::filesystem::{MkdirOptions, mkdir, touch};

    #[test]
    fn finds_manifest_in_start_dir() {
        let dir = tempdir().unwrap();
        touch(dir.path().join(MANIFEST_NAME)).unwrap();
        assert_eq!(
            find_package_root(dir.path()).unwrap(),
            dir.path().to_path_buf()
        );
    }

    #[test]
    fn finds_closest_manifest_in_ancestors() {
        let dir = tempdir().unwrap();
        let inner_pkg = dir.path().join("outer").join("inner");
        let nested = inner_pkg.join("src").join("module");
        mkdir(&nested, MkdirOptions::WithParents).unwrap();
        touch(dir.path().join("outer").join(MANIFEST_NAME)).unwrap();
        touch(inner_pkg.join(MANIFEST_NAME)).unwrap();
        assert_eq!(find_package_root(&nested).unwrap(), inner_pkg);
    }

    #[test]
    fn ignores_directory_named_like_manifest() {
        let dir = tempdir().unwrap();
        let nested = dir.path().join("a");
        mkdir(nested.join(MANIFEST_NAME), MkdirOptions::WithParents).unwrap();
        assert!(find_package_root(&nested).is_err());
    }
}
