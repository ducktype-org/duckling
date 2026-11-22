// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Locating the root of a Duckling package, without parsing its manifest.
//!
//! Ecosystem tools (e.g. a formatter) only need to know where a package lives. Reading the
//! manifest is QuackPack's job.
use std::path::{Path, PathBuf};

use tracing::{debug, trace};

/// File name of a package manifest. The directory holding it is the root of the package.
pub const MANIFEST_NAME: &str = "quackconfig.yaml";

/// Find the root of the package containing `start`.
///
/// Walks up `start` and its ancestors, and returns the first directory holding a
/// [`MANIFEST_NAME`] file, or `None` when there is none. `start` is used as given: it is not
/// made absolute, so pass an absolute path to search up to the filesystem root.
pub fn find_package_root(start: &Path) -> Option<PathBuf> {
    start.ancestors().find_map(|dir| {
        let manifest = dir.join(MANIFEST_NAME);
        trace!("checking for a manifest at `{}`", manifest.display());
        manifest.is_file().then(|| {
            debug!("found a manifest at `{}`", manifest.display());
            dir.to_path_buf()
        })
    })
}

#[cfg(test)]
mod tests {
    use std::fs::{File, create_dir_all};

    use tempfile::tempdir;

    use super::*;

    #[test]
    fn finds_manifest_in_start_dir() {
        let dir = tempdir().unwrap();
        File::create(dir.path().join(MANIFEST_NAME)).unwrap();
        assert_eq!(
            find_package_root(dir.path()),
            Some(dir.path().to_path_buf())
        );
    }

    #[test]
    fn finds_closest_manifest_in_ancestors() {
        let dir = tempdir().unwrap();
        let inner_pkg = dir.path().join("outer").join("inner");
        let nested = inner_pkg.join("src").join("module");
        create_dir_all(&nested).unwrap();
        File::create(dir.path().join("outer").join(MANIFEST_NAME)).unwrap();
        File::create(inner_pkg.join(MANIFEST_NAME)).unwrap();
        assert_eq!(find_package_root(&nested), Some(inner_pkg));
    }

    #[test]
    fn ignores_directory_named_like_manifest() {
        let dir = tempdir().unwrap();
        let nested = dir.path().join("a");
        create_dir_all(nested.join(MANIFEST_NAME)).unwrap();
        assert_eq!(find_package_root(&nested), None);
    }
}
