use std::path::Path;

use tempfile::TempDir;

use crate::DuckContext;
use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::solver::types_common::ExpandedLocation;
use crate::quackpack::core::storage::freeze::{FreezeDep, FreezePackage, RootPackage, VenvFreeze};
use crate::quackpack::core::storage::package_id::RegistryId;
use crate::quackpack::core::{PackageLoader, Version};
use crate::util::path_ops_ext::PathOpsExt;
use crate::util::test_utils::setup_test;

pub fn setup_mock_storage() -> (DuckContext, TempDir) {
    let setup = || {
        let tmpdir_root = TempDir::new().unwrap();
        setup_mock_packages(&tmpdir_root.path().join("storage"));
        setup_mock_root_package(&tmpdir_root.path());
        setup_mock_cycle_package(&tmpdir_root.path());
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
        let pkg_id = RegistryId::new(
            (*name).into(),
            Version::new(1, 0, 0),
            Fetcher::DEFAULT_REGISTRY_URL.parse().unwrap(),
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
    }
    root.try_fsync_dir().unwrap();
}

/// Creates packages for the following scenario:
///
/// ```no_run
///     foo
///    / with feature `use_baz`
///  baz
///
///     bar
///    / with feature `use_baz`
///  baz
///
///  cycle
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
  baz:
    version: 1.0.0
    conditions:
      package-features: [use_baz]
features:
  use_baz: []
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
///     root — cycle if root has cycle
///    /    \
///   foo   bar + use_baz, if root has use_bar_with_baz
///   + use_baz, if root has use_foo_with_baz
/// ```
///
/// There's also an extra feature `nonexistent`.
fn setup_mock_root_package(root: &Path) {
    let pkg_root = root.join("root");
    let cycle_root = root.join("cycle");
    pkg_root.join("src").join("main.duck").touch().unwrap();
    pkg_root
        .join(PackageLoader::MANIFEST_NAME)
        .write(format!(
            "
metadata:
  name: root
  version: 1.0.0

dependencies:
  foo:
    version: 1.0.0
    features:
      - use_baz:
          package-features: [use_foo_with_baz]
      - nonexistent:
          package-features: [nonexistent]
  bar:
    version: 1.0.0
    features:
      - use_baz:
          package-features: [use_bar_with_baz]
  cycle:
    source:
      path: {:?}
    conditions:
      package-features: [cycle]
features:
  use_bar_with_baz: []
  use_foo_with_baz: []
  full: [use_foo_with_baz, use_bar_with_baz]
  nonexistent: []
  cycle: []
        ",
            cycle_root.to_string_lossy()
        ))
        .unwrap();
    pkg_root.try_fsync_dir().unwrap();
}

fn setup_mock_cycle_package(root: &Path) {
    let pkg_root = root.join("cycle");
    let root_root = root.join("root");
    pkg_root.join("src").join("main.duck").touch().unwrap();
    pkg_root
        .join(PackageLoader::MANIFEST_NAME)
        .write(format!(
            "
metadata:
  name: cycle
  version: 1.0.0

dependencies:
  root:
    source:
      path: {:?}
        ",
            root_root.to_string_lossy()
        ))
        .unwrap();
    pkg_root.try_fsync_dir().unwrap();
}

/// Generate mock [`VenvFreeze`].
///
/// This function should be generally used in order to create
/// [`BuildContext`](super::BuildContext).
pub fn freeze(root: &Path) -> VenvFreeze {
    VenvFreeze::new(
        RootPackage::new(
            "root".into(),
            Version::new(1, 0, 0),
            vec![
                "use_bar_with_baz".into(),
                "full".into(),
                "nonexistent".into(),
                "use_foo_with_baz".into(),
                "cycle".into(),
            ],
            vec![
                FreezeDep::new("foo".into(), Version::new(1, 0, 0)),
                FreezeDep::new("bar".into(), Version::new(1, 0, 0)),
                FreezeDep::new("cycle".into(), Version::new(1, 0, 0)),
            ],
        ),
        vec![
            FreezePackage::new(
                "foo".into(),
                Version::new(1, 0, 0),
                vec!["use_baz".into()],
                vec![FreezeDep::new("baz".into(), Version::new(1, 0, 0))],
                ExpandedLocation::Registry {
                    url: Fetcher::DEFAULT_REGISTRY_URL.parse().unwrap(),
                    real_name: "foo".into(),
                }
                .into(),
            ),
            FreezePackage::new(
                "bar".into(),
                Version::new(1, 0, 0),
                vec!["use_baz".into()],
                vec![FreezeDep::new("baz".into(), Version::new(1, 0, 0))],
                ExpandedLocation::Registry {
                    url: Fetcher::DEFAULT_REGISTRY_URL.parse().unwrap(),
                    real_name: "bar".into(),
                }
                .into(),
            ),
            FreezePackage::new(
                "baz".into(),
                Version::new(1, 0, 0),
                vec![],
                vec![],
                ExpandedLocation::Registry {
                    url: Fetcher::DEFAULT_REGISTRY_URL.parse().unwrap(),
                    real_name: "baz".into(),
                }
                .into(),
            ),
            FreezePackage::new(
                "cycle".into(),
                Version::new(1, 0, 0),
                vec![],
                vec![FreezeDep::new("root".into(), Version::new(1, 0, 0))],
                ExpandedLocation::Local {
                    absolute_path: root.join("cycle"),
                }
                .into(),
            ),
        ],
    )
}

pub fn freeze_without_direct_dep() -> VenvFreeze {
    VenvFreeze::new(
        RootPackage::new(
            "root".into(),
            Version::new(1, 0, 0),
            vec![
                "use_bar_with_baz".into(),
                "full".into(),
                "nonexistent".into(),
                "use_foo_with_baz".into(),
            ],
            vec![
                FreezeDep::new("foo".into(), Version::new(1, 0, 0)),
                FreezeDep::new("bar".into(), Version::new(1, 0, 0)),
            ],
        ),
        vec![],
    )
}

pub fn freeze_without_transitive_dep() -> VenvFreeze {
    VenvFreeze::new(
        RootPackage::new(
            "root".into(),
            Version::new(1, 0, 0),
            vec![
                "use_bar_with_baz".into(),
                "full".into(),
                "nonexistent".into(),
                "use_foo_with_baz".into(),
            ],
            vec![
                FreezeDep::new("foo".into(), Version::new(1, 0, 0)),
                FreezeDep::new("bar".into(), Version::new(1, 0, 0)),
            ],
        ),
        vec![
            FreezePackage::new(
                "foo".into(),
                Version::new(1, 0, 0),
                vec!["use_baz".into()],
                vec![FreezeDep::new("baz".into(), Version::new(1, 0, 0))],
                ExpandedLocation::Registry {
                    url: Fetcher::DEFAULT_REGISTRY_URL.parse().unwrap(),
                    real_name: "foo".into(),
                }
                .into(),
            ),
            FreezePackage::new(
                "bar".into(),
                Version::new(1, 0, 0),
                vec!["use_baz".into()],
                vec![FreezeDep::new("foo".into(), Version::new(1, 0, 0))],
                ExpandedLocation::Registry {
                    url: Fetcher::DEFAULT_REGISTRY_URL.parse().unwrap(),
                    real_name: "bar".into(),
                }
                .into(),
            ),
        ],
    )
}
