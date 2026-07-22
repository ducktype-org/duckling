use std::path::Path;

use tempfile::TempDir;

use crate::DuckContext;
use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
use crate::quackpack::core::identity::{Identity, Origin};
use crate::quackpack::core::storage::freeze::{FreezePackage, RootPackage, VenvFreeze};
use crate::quackpack::core::{PackageId, PackageLoader, Version};
use crate::quackpack::util::to_url::ToUrl;
use crate::util::path_ops_ext::PathOpsExt;
use crate::util::test_utils::setup_test;

pub fn setup_mock_storage() -> (DuckContext, TempDir) {
    let setup = || {
        let tmpdir_root = TempDir::new().unwrap();
        setup_mock_packages(&tmpdir_root.path().join("storage"));
        setup_mock_root_package(tmpdir_root.path());
        setup_mock_cycle_package(tmpdir_root.path());
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
      path: {}
    conditions:
      package-features: [cycle]
features:
  use_bar_with_baz: []
  use_foo_with_baz: []
  full: [use_foo_with_baz, use_bar_with_baz]
  nonexistent: []
  cycle: []
        ",
            cycle_root.display()
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
      path: {}
        ",
            root_root.display()
        ))
        .unwrap();
    pkg_root.try_fsync_dir().unwrap();
}

/// Generate mock [`VenvFreeze`].
///
/// This function should be generally used in order to create
/// [`BuildContext`](super::BuildContext).
pub fn freeze(root: &Path) -> VenvFreeze {
    let full_origin = FullOrigin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
    let origin = Origin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
    let local_origin = |name: &str| Origin::for_local(&root.join(name)).unwrap();
    let full_local_origin = |name: &str| FullOrigin::for_local(&root.join(name)).unwrap();

    let mock_local_identity = |name: &str| Identity::new(name.into(), local_origin(name));
    let mock_local_full_identity =
        |name: &str| FullIdentity::new(name.into(), full_local_origin(name));
    let mock_identity = |name: &str| Identity::new(name.into(), origin);
    let mock_full_identity = |name: &str| FullIdentity::new(name.into(), full_origin);

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
                mock_identity("foo"),
                mock_identity("bar"),
                mock_local_identity("cycle"),
            ],
        ),
        vec![
            FreezePackage::new(
                mock_full_identity("foo"),
                Version::new(1, 0, 0),
                vec!["use_baz".into()],
                vec![mock_identity("baz")],
            ),
            FreezePackage::new(
                mock_full_identity("bar"),
                Version::new(1, 0, 0),
                vec!["use_baz".into()],
                vec![mock_identity("baz")],
            ),
            FreezePackage::new(
                mock_full_identity("baz"),
                Version::new(1, 0, 0),
                vec![],
                vec![],
            ),
            FreezePackage::new(
                mock_local_full_identity("cycle"),
                Version::new(1, 0, 0),
                vec![],
                vec![mock_local_identity("root")],
            ),
        ],
    )
}

pub fn freeze_without_direct_dep() -> VenvFreeze {
    let origin = Origin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
    let mock_identity = |name: &str| Identity::new(name.into(), origin);
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
            vec![mock_identity("foo"), mock_identity("bar")],
        ),
        vec![],
    )
}

pub fn freeze_without_transitive_dep() -> VenvFreeze {
    let full_origin = FullOrigin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
    let origin = Origin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
    let mock_identity = |name: &str| Identity::new(name.into(), origin);
    let mock_full_identity = |name: &str| FullIdentity::new(name.into(), full_origin);
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
            vec![mock_identity("foo"), mock_identity("bar")],
        ),
        vec![
            FreezePackage::new(
                mock_full_identity("foo"),
                Version::new(1, 0, 0),
                vec!["use_baz".into()],
                vec![mock_identity("baz")],
            ),
            FreezePackage::new(
                mock_full_identity("bar"),
                Version::new(1, 0, 0),
                vec!["use_baz".into()],
                vec![mock_identity("foo")],
            ),
        ],
    )
}
