use std::{
    path::{Path, PathBuf},
    time::{Duration, SystemTime},
};

use crate::{
    quackpack::core::{
        fetcher::Fetcher,
        storage::{
            freeze::{FreezeDep, FreezePackage},
            package_id::{PackageId, RegistryId},
            paths::Storage,
            venv::Venv,
            venv_id::ToVenvId,
        },
        types_common::ExpandedLocation,
    },
    util::{path_ops_ext::PathOpsExt, test_utils::setup_test},
};
use tempfile::TempDir;
use url::Url;

use crate::{
    DuckContext,
    quackpack::core::{
        Version,
        storage::{
            freeze::{RootPackage, VenvFreeze},
            venv::VenvData,
        },
    },
};

mod basic;
mod concurrent;

fn registry_url_hash() -> String {
    let url: Url = Fetcher::DEFAULT_REGISTRY_URL.parse().unwrap();
    crate::util::hash::sha256_string(url.host_str().unwrap())
}

/// This function creates mock storage with following contents:
/// 1. We have three packages: foo, bar, and baz.
/// 2. We have four venvs:
///   1. `root1`: not ephemeral, without dependencies,
///   2. `root2`: ephemeral, without dependencies, used now
///   3. `root3`: ephemeral, with dependency bar, old enough to be removed during clean.
///   4. `root4`: not ephemeral, old, with dependency baz.
/// 3. All possible locks.
///
/// Clean should:
/// 1. remove foo, because no package references it,
/// 2. remove root3, because it's too old,
/// 3. remove bar, because it was only references by root3.
fn setup_mock_storage() -> (DuckContext, TempDir) {
    let setup = || {
        let storage_root = TempDir::new().unwrap();
        setup_mock_packages(storage_root.path());
        setup_mock_venvs(storage_root.path());
        setup_mock_locks(storage_root.path());
        // Also overwrite DUCK_HOME, so we'll use the default configuration options.
        // SAFETY: Setup is single threaded, and `Env` in `DuckContext`, copies all envs.
        unsafe {
            std::env::set_var("DUCK_STORAGE_DIR", storage_root.path());
            std::env::set_var("DUCK_HOME", storage_root.path());
        }
        let ctx = DuckContext::default();
        // SAFETY: Setup is single threaded, and `Env` in `DuckContext`, copies all envs.
        unsafe {
            std::env::remove_var("DUCK_STORAGE_DIR");
            std::env::remove_var("DUCK_HOME");
        }
        (ctx, storage_root)
    };
    setup_test(setup)
}

fn setup_mock_packages(root: &Path) {
    let names = ["foo", "bar", "baz"];
    for name in names {
        let name = PackageId::Registry(RegistryId::new(
            name.into(),
            Version::new(1, 0, 0),
            Fetcher::DEFAULT_REGISTRY_URL.parse().unwrap(),
        ));
        root.join("pkg")
            .join(name.storage_name())
            .join("src")
            .join("main.duck")
            .touch()
            .unwrap();
    }
}

fn setup_mock_venv(
    root: &Path,
    name: &str,
    freeze_mutator: impl FnOnce(&mut VenvFreeze),
    data_mutator: impl FnOnce(&mut VenvData),
) {
    let storage = Storage::new(root);
    let mut basic_freeze = VenvFreeze::new(
        RootPackage::new(name.into(), Version::new(1, 0, 0), vec![], vec![]),
        vec![],
    );
    freeze_mutator(&mut basic_freeze);
    let mut basic_data = VenvData::new(
        basic_freeze,
        false,
        PathBuf::default(),
        SystemTime::now(),
        SystemTime::now(),
    );
    data_mutator(&mut basic_data);
    let venv = Venv::new(name.to_venv_id(), basic_data);
    venv.save_to(&storage).unwrap()
}

fn setup_mock_venvs(root: &Path) {
    setup_mock_venv(root, "root1", |_| {}, |_| {});
    setup_mock_venv(
        root,
        "root2",
        |_| {},
        |data| {
            data.set_ephemeral(true);
        },
    );
    let dep = FreezeDep::new("bar".into(), Version::new(1, 0, 0));
    let package = FreezePackage::new(
        dep.name(),
        dep.version(),
        vec![],
        vec![],
        ExpandedLocation::Registry {
            url: Fetcher::DEFAULT_REGISTRY_URL.parse().unwrap(),
            real_name: dep.name(),
        }
        .into(),
    );
    setup_mock_venv(
        root,
        "root3",
        |freeze| {
            freeze.root_mut().dependencies_mut().push(dep);
            freeze.dependencies_mut().push(package);
        },
        |data| {
            data.set_ephemeral(true);
            data.set_last_modification(SystemTime::now() - Duration::from_secs(2 * 24 * 60 * 60));
        },
    );

    let dep = FreezeDep::new("baz".into(), Version::new(1, 0, 0));
    let package = FreezePackage::new(
        dep.name(),
        dep.version(),
        vec![],
        vec![],
        ExpandedLocation::Registry {
            url: Fetcher::DEFAULT_REGISTRY_URL.parse().unwrap(),
            real_name: dep.name(),
        }
        .into(),
    );

    setup_mock_venv(
        root,
        "root4",
        |freeze| {
            freeze.root_mut().dependencies_mut().push(dep);
            freeze.dependencies_mut().push(package);
        },
        |data| {
            data.set_last_modification(SystemTime::now() - Duration::from_secs(2 * 24 * 60 * 60));
        },
    );
}

fn setup_mock_locks(root: &Path) {
    let setup_compile_locks = |names: &[&str]| {
        for name in names {
            root.join("locks")
                .join("compile")
                .join(name)
                .touch()
                .unwrap();
        }
    };

    let setup_sync_locks = |names: &[&str]| {
        for name in names {
            root.join("locks")
                .join("venv_sync")
                .join(name)
                .touch()
                .unwrap();
        }
    };

    let setup_data_locks = |names: &[&str]| {
        for name in names {
            root.join("locks")
                .join("venv_data")
                .join(name)
                .touch()
                .unwrap();
        }
    };
    root.join("locks").join("clean.lock").touch().unwrap();
    let venvs = ["root1", "root2", "root3", "root4"];
    setup_data_locks(&venvs);
    setup_sync_locks(&venvs);
    setup_compile_locks(&venvs);
}
