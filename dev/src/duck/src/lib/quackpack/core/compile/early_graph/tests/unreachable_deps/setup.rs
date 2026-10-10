// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::path::Path;

use tempfile::TempDir;

use crate::DuckContext;
use crate::quackpack::core::compile::early_graph::tests::{mock_local_pkg, mock_registry_pkg};
use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
use crate::quackpack::core::solver::solver_freeze::{SolverFreeze, SolverPackageFreeze};
use crate::quackpack::core::storage::paths;
use crate::quackpack::core::{PackageId, PackageLoader, Version};
use crate::quackpack::util::to_url::ToUrl;
use crate::util::path_ops_ext::PathOpsExt;
use crate::util::test_utils::setup_test;

pub fn setup_mock_storage() -> (DuckContext, TempDir) {
    let setup = || {
        let tmpdir_root = TempDir::new().unwrap();
        setup_mock_packages(&tmpdir_root.path().join("storage"));
        setup_mock_root_package(tmpdir_root.path());
        // Also overwrite DUCK_HOME, so we'll use the default configuration options.
        // SAFETY: Setup is single threaded, and `Env` in `DuckContext`, copies all envs.
        unsafe {
            std::env::set_var("DUCK_HOME", tmpdir_root.path());
        }
        let ctx = DuckContext::default();
        // SAFETY: Setup is single threaded, and `Env` in `DuckContext`, copies all envs.
        unsafe {
            std::env::remove_var("DUCK_HOME");
        }
        (ctx, tmpdir_root)
    };
    setup_test(setup)
}

pub fn setup_mock_packages(root: &Path) {
    for (name, manifest) in packages_names_and_manifests() {
        let mock_registry_pkg = PackageId::new(
            FullIdentity::new(
                (*name).into(),
                FullOrigin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap()),
            ),
            Version::new(1, 0, 0),
        );
        root.join("pkg")
            .join(mock_registry_pkg.storage_name())
            .join("src")
            .join("main.duck")
            .touch()
            .unwrap();

        root.join("pkg")
            .join(mock_registry_pkg.storage_name())
            .join(PackageLoader::MANIFEST_NAME)
            .write(manifest)
            .unwrap();

        root.join("pkg")
            .join(mock_registry_pkg.storage_name())
            .join(paths::OK_FILENAME)
            .touch()
            .unwrap();
    }
    root.try_fsync_dir().unwrap();
}

/// `bar` pulls in `baz`.
/// Should be use with [`freeze`] freeze.
fn packages_names_and_manifests() -> &'static [(&'static str, &'static str)] {
    &[
        (
            "bar",
            "
metadata:
  name: bar
  version: 1.0.0

dependencies:
  baz:
    version: 1.0.0
",
        ),
        (
            "baz",
            "
metadata:
  name: baz
  version: 1.0.0
",
        ),
    ]
}

/// `root` depends conditionally on `bar`. Removing `bar` should also remove `baz`.
fn setup_mock_root_package(root: &Path) {
    let pkg_root = root.join("root");
    pkg_root.join("src").join("main.duck").touch().unwrap();
    pkg_root
        .join(PackageLoader::MANIFEST_NAME)
        .write(
            "
metadata:
  name: root
  version: 1.0.0

dependencies:
  bar:
    version: 1.0.0
    conditions:
      package-features: [use-bar]
features:
  use-bar: []
        ",
        )
        .unwrap();
    pkg_root.try_fsync_dir().unwrap();
}

/// Generate mock [`SolverFreeze`].
///
/// This function should be generally used in order to create
/// [`BuildContext`](super::BuildContext).
pub fn freeze(root: &Path) -> SolverFreeze {
    SolverFreeze {
        main_pkg: mock_local_pkg(root, "root"),
        package_freezes: [
            (
                mock_local_pkg(root, "root"),
                SolverPackageFreeze {
                    features: ["use-bar".into()].into(),
                    dependencies_realization: [("bar".into(), mock_registry_pkg("bar"))].into(),
                },
            ),
            (
                mock_registry_pkg("bar"),
                SolverPackageFreeze {
                    features: [].into(),
                    dependencies_realization: [("baz".into(), mock_registry_pkg("baz"))].into(),
                },
            ),
            (mock_registry_pkg("baz"), SolverPackageFreeze::new()),
        ]
        .into(),
    }
}
