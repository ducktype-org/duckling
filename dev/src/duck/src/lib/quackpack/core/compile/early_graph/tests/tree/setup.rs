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

/// Generate mock [`VenvFreeze`].
///
/// This function should be generally used in order to create
/// [`BuildContext`](super::BuildContext).
pub fn freeze() -> VenvFreeze {
    VenvFreeze::new(
        RootPackage::new(
            "root".into(),
            Version::new(1, 0, 0),
            vec!["use_bar".into(), "full".into(), "nonexistent".into()],
            vec![FreezeDep::new("foo".into(), Version::new(1, 0, 0))],
        ),
        vec![
            FreezePackage::new(
                "foo".into(),
                Version::new(1, 0, 0),
                vec!["use_bar".into(), "use_bar_with_baz".into()],
                vec![FreezeDep::new("bar".into(), Version::new(1, 0, 0))],
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
        ],
    )
}

pub fn freeze_without_direct_dep() -> VenvFreeze {
    VenvFreeze::new(
        RootPackage::new(
            "root".into(),
            Version::new(1, 0, 0),
            vec!["use_bar".into(), "full".into(), "nonexistent".into()],
            vec![FreezeDep::new("foo".into(), Version::new(1, 0, 0))],
        ),
        vec![],
    )
}

pub fn freeze_without_transitive_dep() -> VenvFreeze {
    VenvFreeze::new(
        RootPackage::new(
            "root".into(),
            Version::new(1, 0, 0),
            vec!["use_bar".into(), "full".into(), "nonexistent".into()],
            vec![FreezeDep::new("foo".into(), Version::new(1, 0, 0))],
        ),
        vec![FreezePackage::new(
            "foo".into(),
            Version::new(1, 0, 0),
            vec!["use_bar".into(), "use_bar_with_baz".into()],
            vec![FreezeDep::new("bar".into(), Version::new(1, 0, 0))],
            ExpandedLocation::Registry {
                url: Fetcher::DEFAULT_REGISTRY_URL.parse().unwrap(),
                real_name: "foo".into(),
            }
            .into(),
        )],
    )
}
