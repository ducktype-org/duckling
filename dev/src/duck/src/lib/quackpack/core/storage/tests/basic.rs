use std::path::Path;

use super::{registry_url_hash, setup_mock_storage};
use crate::StrId;
use crate::quackpack::core::Version;
use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::solver::types_common::ExpandedLocation;
use crate::quackpack::core::storage::freeze::FreezePackage;
use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::core::storage::venv::Venv;
use crate::quackpack::core::storage::venv_id::ToVenvId;
use crate::quackpack::core::storage::{self};
use crate::util::path_ops_ext::PathOpsExt;

fn check_venvs_exist(root: &Path, names: &[&str]) {
    for name in names {
        assert!(root.join("venv").join(name).exists());
        assert!(root.join("locks").join("compile").join(name).exists());
        assert!(root.join("locks").join("venv_sync").join(name).exists());
        assert!(root.join("locks").join("venv_data").join(name).exists());
    }
}

fn check_venvs_dont_exist(root: &Path, names: &[&str]) {
    for name in names {
        assert!(!root.join("venv").join(name).exists());
        assert!(!root.join("locks").join("compile").join(name).exists());
        assert!(!root.join("locks").join("venv_sync").join(name).exists());
        assert!(!root.join("locks").join("venv_data").join(name).exists());
    }
}

#[test]
/// Check that we clean what we should've cleaned.
/// Details are in [`setup_mock_storage`].
fn clean() {
    let (ctx, root) = setup_mock_storage();
    let mut output = storage::ops::clean_storage(&ctx, ctx.default_storage_root()).unwrap();
    output.removed_packages.sort();
    let mut expected_packages = [
        root.path()
            .join("pkg")
            .join(format!("registry-{}-foo-1.0.0", registry_url_hash())),
        root.path()
            .join("pkg")
            .join(format!("registry-{}-bar-1.0.0", registry_url_hash())),
    ];
    expected_packages.sort();
    assert_eq!(output.removed_venvs, ["root3".to_venv_id()]);
    assert_eq!(output.removed_packages, expected_packages);
    check_venvs_exist(root.path(), &["root1", "root2", "root4"]);
    check_venvs_dont_exist(root.path(), &["root3"]);

    let mut iterator = root.path().join("pkg").read_dir().unwrap();
    assert!(
        iterator.next().is_some_and(|entry| entry.unwrap().path()
            == root
                .path()
                .join("pkg")
                .join(format!("registry-{}-baz-1.0.0", registry_url_hash()))),
        "we should left only baz"
    );

    assert!(iterator.next().is_none(), "we should left only baz");
}

#[test]
/// Check, that when deleting venv we only delete it.
/// Also, after deleting every venv, check that clean removes every package, but no venvs are
/// removed.
fn delete_venv() {
    let (ctx, root) = setup_mock_storage();
    let storage_root = ctx.default_storage_root();
    storage::ops::delete_venv(&ctx, storage_root, StrId::new("root1")).unwrap();
    check_venvs_exist(root.path(), &["root2", "root3", "root4"]);
    check_venvs_dont_exist(root.path(), &["root1"]);
    storage::ops::delete_venv(&ctx, storage_root, StrId::new("root2")).unwrap();
    check_venvs_exist(root.path(), &["root3", "root4"]);
    check_venvs_dont_exist(root.path(), &["root1", "root2"]);
    storage::ops::delete_venv(&ctx, storage_root, StrId::new("root3")).unwrap();
    check_venvs_exist(root.path(), &["root4"]);
    check_venvs_dont_exist(root.path(), &["root1", "root2", "root3"]);

    storage::ops::delete_venv(&ctx, storage_root, StrId::new("non_existent_venv")).unwrap();
    check_venvs_exist(root.path(), &["root4"]);
    check_venvs_dont_exist(root.path(), &["root1", "root2", "root3"]);

    storage::ops::delete_venv(&ctx, storage_root, StrId::new("root4")).unwrap();
    check_venvs_dont_exist(root.path(), &["root1", "root2", "root3", "root4"]);

    let mut output = storage::ops::clean_storage(&ctx, storage_root).unwrap();
    output.removed_packages.sort();
    let mut expected_packages = [
        root.path()
            .join("pkg")
            .join(format!("registry-{}-foo-1.0.0", registry_url_hash())),
        root.path()
            .join("pkg")
            .join(format!("registry-{}-baz-1.0.0", registry_url_hash())),
        root.path()
            .join("pkg")
            .join(format!("registry-{}-bar-1.0.0", registry_url_hash())),
    ];
    expected_packages.sort();
    assert_eq!(output.removed_packages, expected_packages);
    assert!(output.removed_venvs.is_empty());
}

#[test]
/// Basic info output tests.
fn info() {
    let (ctx, _root) = setup_mock_storage();
    let output = storage::ops::list_venvs(ctx.default_storage_root()).unwrap();
    assert_eq!(output.len(), 4);
    assert_eq!(
        output
            .get(&"root1".to_venv_id())
            .unwrap()
            .data()
            .freeze()
            .root()
            .name(),
        "root1"
    );

    assert_eq!(
        output
            .get(&"root1".to_venv_id())
            .unwrap()
            .data()
            .freeze()
            .root()
            .version(),
        Version::new(1, 0, 0)
    );

    assert_eq!(
        output
            .get(&"root2".to_venv_id())
            .unwrap()
            .data()
            .freeze()
            .root()
            .name(),
        "root2"
    );

    assert_eq!(
        output
            .get(&"root3".to_venv_id())
            .unwrap()
            .data()
            .freeze()
            .root()
            .name(),
        "root3"
    );

    assert_eq!(
        output
            .get(&"root4".to_venv_id())
            .unwrap()
            .data()
            .freeze()
            .root()
            .name(),
        "root4"
    );

    assert_eq!(
        output
            .get(&"root2".to_venv_id())
            .unwrap()
            .data()
            .freeze()
            .root()
            .version(),
        Version::new(1, 0, 0)
    );

    assert_eq!(
        output
            .get(&"root3".to_venv_id())
            .unwrap()
            .data()
            .freeze()
            .root()
            .version(),
        Version::new(1, 0, 0)
    );

    assert_eq!(
        output
            .get(&"root3".to_venv_id())
            .unwrap()
            .data()
            .freeze()
            .root()
            .dependencies()
            .len(),
        1
    );

    assert!(
        output
            .get(&"root2".to_venv_id())
            .unwrap()
            .data()
            .freeze()
            .dependencies()
            .is_empty()
    );

    assert!(
        output
            .get(&"root1".to_venv_id())
            .unwrap()
            .data()
            .freeze()
            .root()
            .dependencies()
            .is_empty()
    );
}

fn assert_can_load_after_save(venv: &Venv, storage: &Storage) -> Venv {
    venv.save_to(storage).unwrap();
    let id = venv.id();
    let metadata = storage.venv_metadata(id);
    let backup = storage.venv_backup_metadata(id);
    let metadata_contents = metadata.read_to_string().unwrap();
    let backup_contents = backup.read_to_string().unwrap();
    assert_eq!(
        metadata_contents,
        backup_contents,
        "different contents of venv `{id}`; (metadata: `{}`, backup: `{}`)",
        metadata.display(),
        backup.display()
    );
    Venv::fix_and_load(storage, id)
        .expect("an error occured")
        .expect("failed to load venv")
}

#[test]
fn save_trims_files() {
    let (ctx, _root) = setup_mock_storage();
    let id = "root1".to_venv_id();
    let storage = Storage::new(ctx.default_storage_root());
    let mut venv = Venv::fix_and_load(&storage, id)
        .expect("an error occured")
        .expect("failed to load venv");
    let original_venv = venv.clone();
    // Firstly, add a lot of dependencies, to make a file longer (have more bytes).
    let number_of_new_packages = 5;
    for _ in 0..number_of_new_packages {
        let package = FreezePackage::new(
            "dep".into(),
            Version::new(1, 0, 0),
            vec![],
            vec![],
            ExpandedLocation::Registry {
                url: Fetcher::DEFAULT_REGISTRY_URL.parse().unwrap(),
                real_name: "dep".into(),
            }
            .into(),
        );
        venv.data_mut()
            .freeze_mut()
            .dependencies_mut()
            .push(package);
    }
    let mut venv = assert_can_load_after_save(&venv, &storage);
    // Secondly, remove all dependencies, to make a file shorter, so we'll leave trailing bytes (which should be truncated).
    venv.data_mut().freeze_mut().dependencies_mut().clear();
    let mut new_venv = assert_can_load_after_save(&venv, &storage);
    // The first and last state should only differ in access time. Copy one from the other.
    new_venv
        .data_mut()
        .set_last_access(original_venv.data().last_access());
    assert_eq!(new_venv, original_venv);
}
