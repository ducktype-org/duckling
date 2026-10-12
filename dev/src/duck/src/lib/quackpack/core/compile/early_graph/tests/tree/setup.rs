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
use crate::quackpack::core::{PackageId, PackageLoader, Version, storage};
use crate::quackpack::util::to_url::ToUrl;
use crate::util::path_ops_ext::PathOpsExt;
use crate::util::test_utils::setup_test;

pub fn setup_mock_storage() -> (DuckContext, TempDir) {
    let setup = || {
        let tmpdir_root = TempDir::new().unwrap();
        setup_mock_packages(&tmpdir_root.path().join("storage"));
        setup_mock_root_package(&tmpdir_root.path().join("root"));
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
        let pkg_id = PackageId::new(
            FullIdentity::new(
                (*name).into(),
                FullOrigin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap()),
            ),
            Version::new(1, 0, 0),
        );
        root.join("pkg")
            .join(pkg_id.storage_name())
            .join("src")
            .join("main.duck")
            .touch()
            .unwrap();

        root.join("pkg")
            .join(pkg_id.storage_name())
            .join(PackageLoader::MANIFEST_NAME)
            .write(manifest)
            .unwrap();

        root.join("pkg")
            .join(pkg_id.storage_name())
            .join(storage::paths::OK_FILENAME)
            .touch()
            .unwrap();
    }
    root.try_fsync_dir().unwrap();
}

/// Creates packages for the following scenario:
///
/// ```no_run
///     foo
///    / with feature `use_bar`
///   bar
///  / with feature `use_baz`; enabled by `use_bar_with_baz` in foo
/// baz
/// ```
/// Should be use with [`freeze`] freeze.
fn packages_names_and_manifests() -> &'static [(&'static str, &'static str)] {
    &[
        (
            "foo",
            "
metadata:
  name: foo
  version: 1.0.0

dependencies:
  bar:
    version: 1.0.0
    conditions:
      package-features: [use_bar]
    features:
      - use_baz:
          package-features: [use_bar_with_baz]
features:
  use_bar: []
  use_bar_with_baz: [use_bar]
",
        ),
        (
            "bar",
            "
metadata:
  name: bar
  version: 1.0.0

dependencies:
  baz:
    version: 1.0.0
    conditions:
      package-features: [use_baz]
features:
  use_baz: []
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

/// Setup mock root package with a following scenario:
///
/// ```no_run
///     root
///    /
///   foo + use_bar, if root has use_bar
///       + use_bar_with_baz + use_bar, if root has full
///       + use_bar_with_baz, if root has baz_without_bar
/// ```
///
/// There's also an extra feature `nonexistent`.
fn setup_mock_root_package(root: &Path) {
    root.join("src").join("main.duck").touch().unwrap();
    root.join(PackageLoader::MANIFEST_NAME)
        .write(
            "
metadata:
  name: root
  version: 1.0.0

dependencies:
  foo:
    version: 1.0.0
    features:
      - use_bar:
          package-features: [use_bar]
      - use_bar_with_baz:
          package-features: [full, baz_without_bar]
      - nonexistent:
          package-features: [nonexistent]
features:
  use_bar: []
  full: [use_bar]
  baz_without_bar: []
  nonexistent: []
        ",
        )
        .unwrap();
    root.try_fsync_dir().unwrap();
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
                    features: ["use_bar".into(), "full".into(), "nonexistent".into()].into(),
                    dependencies_realization: [("foo".into(), mock_registry_pkg("foo"))].into(),
                },
            ),
            (
                mock_registry_pkg("foo"),
                SolverPackageFreeze {
                    features: ["use_bar".into(), "use_bar_with_baz".into()].into(),
                    dependencies_realization: [("bar".into(), mock_registry_pkg("bar"))].into(),
                },
            ),
            (
                mock_registry_pkg("bar"),
                SolverPackageFreeze {
                    features: ["use_baz".into()].into(),
                    dependencies_realization: [("baz".into(), mock_registry_pkg("baz"))].into(),
                },
            ),
            (mock_registry_pkg("baz"), SolverPackageFreeze::new()),
        ]
        .into(),
    }
}
