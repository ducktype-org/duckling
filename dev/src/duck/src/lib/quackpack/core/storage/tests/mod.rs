use std::fs::OpenOptions;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::time::{Duration, SystemTime};

use tempfile::TempDir;

use crate::DuckContext;
use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
use crate::quackpack::core::identity::{Identity, Origin};
use crate::quackpack::core::storage::freeze::{FreezePackage, RootPackage, VenvFreeze};
use crate::quackpack::core::storage::package_id::{PackageId, RegistryId};
use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::core::storage::venv::{Venv, VenvData};
use crate::quackpack::core::storage::venv_id::ToVenvId;
use crate::quackpack::core::{PackageContext, PackageLoader, Version};
use crate::quackpack::subcommands::init;
use crate::quackpack::subcommands::init::InitOptions;
use crate::quackpack::util::to_url::ToUrl;
use crate::util::path_ops_ext::PathOpsExt;
use crate::util::test_utils::setup_test;

mod basic;
mod concurrent;

fn registry_url_hash() -> String {
    let url = Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap();
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
fn setup_mock_storage() -> (DuckContext, TempDir, PathBuf) {
    let setup = || {
        let duck_home = TempDir::new().unwrap();
        // Also overwrite DUCK_HOME, so we'll use the default configuration options.
        // SAFETY: Setup is single threaded, and `Env` in `DuckContext`, copies all envs.
        unsafe {
            std::env::set_var("DUCK_HOME", duck_home.path());
        }
        let ctx = DuckContext::default();
        // SAFETY: Setup is single threaded, and `Env` in `DuckContext`, copies all envs.
        unsafe {
            std::env::remove_var("DUCK_HOME");
        }
        let storage_root = ctx.duck_home().storage().into_not_locked_path();
        setup_mock_packages(storage_root.as_path());
        setup_mock_venvs(storage_root.as_path(), &ctx);
        setup_mock_locks(storage_root.as_path());
        (ctx, duck_home, storage_root)
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
    ctx: &DuckContext,
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
    venv.save_to(&storage, ctx).unwrap()
}

fn setup_mock_venvs(root: &Path, ctx: &DuckContext) {
    setup_mock_venv(root, "root1", |_| {}, |_| {}, ctx);
    setup_mock_venv(
        root,
        "root2",
        |_| {},
        |data| {
            data.set_ephemeral(true);
        },
        ctx,
    );

    let origin = FullOrigin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
    let simple_origin = Origin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
    let mock_simple_identity = |name: &str| Identity::new(name.into(), simple_origin);
    let mock_identity = |name: &str| FullIdentity::new(name.into(), origin);
    let dep = mock_simple_identity("bar");
    let package = FreezePackage::new(
        mock_identity(dep.name().as_str()),
        Version::new(1, 0, 0),
        vec![],
        vec![],
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
        ctx,
    );

    let dep = mock_simple_identity("baz");
    let package = FreezePackage::new(
        mock_identity(dep.name().as_str()),
        Version::new(1, 0, 0),
        vec![],
        vec![],
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
        ctx,
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

fn create_mock_package<'duck>(
    root: &Path,
    ctx: &'duck DuckContext,
    name: &str,
) -> PackageContext<'duck> {
    let opts = InitOptions {
        ctx,
        at: root.to_path_buf(),
        explicit_name: Some(name),
        as_venv: false,
        expose_freezefile: false,
        ephemeral: false,
        local_storage: false,
        git: false,
        full: false,
    };
    init::init(opts).unwrap();
    PackageLoader::find_at_exact_directory(root, ctx).unwrap()
}

fn create_mock_package_at_tmpdir<'duck>(
    ctx: &'duck DuckContext,
    name: &str,
) -> (TempDir, PackageContext<'duck>) {
    let root = TempDir::new().unwrap();
    let pcx = create_mock_package(root.path(), ctx, name);
    (root, pcx)
}

fn create_mock_package_with_dependencies<'duck>(
    root: &Path,
    ctx: &'duck DuckContext,
    name: &str,
) -> PackageContext<'duck> {
    let opts = InitOptions {
        ctx,
        at: root.join("dep"),
        explicit_name: Some("dep"),
        as_venv: false,
        expose_freezefile: false,
        ephemeral: false,
        local_storage: false,
        git: false,
        full: false,
    };
    init::init(opts).unwrap();

    let opts = InitOptions {
        ctx,
        at: root.join("root"),
        explicit_name: Some(name),
        as_venv: false,
        expose_freezefile: false,
        ephemeral: false,
        local_storage: false,
        git: false,
        full: false,
    };
    init::init(opts).unwrap();
    // !TODO: Use `duck add`.
    let mut file = {
        let mut opts = OpenOptions::new();
        opts.append(true)
            .open(root.join("root").join(PackageLoader::MANIFEST_NAME))
            .unwrap()
    };
    file.write_all(
        "
dependencies:
  dep:
    source:
      path: ../dep"
            .as_bytes(),
    )
    .unwrap();
    file.flush().unwrap();
    file.sync_data().unwrap();
    drop(file);
    PackageLoader::find_at_exact_directory(&root.join("root"), ctx).unwrap()
}

fn create_mock_package_with_deps_at_tmpdir<'duck>(
    ctx: &'duck DuckContext,
    name: &str,
) -> (TempDir, PackageContext<'duck>) {
    let root = TempDir::new().unwrap();
    let pcx = create_mock_package_with_dependencies(root.path(), ctx, name);
    (root, pcx)
}
