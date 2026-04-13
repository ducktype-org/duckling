//! Loading packages from the disk.
use std::{io, marker::PhantomData, path::Path};

use tracing::{debug, trace};

use crate::{
    DuckContext, QuackResult, QuackResultContext,
    duck::util::duck_home::DuckHome,
    qp_bail, qp_err,
    quackpack::core::{
        PackageContext,
        storage::{paths::Storage, venv::Venv, venv_id::VenvId},
    },
    util::path_ops_ext::PathOpsExt,
};

#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
/// Is [`PackageLoader`] allowed to return a global package, if it doesn't find any package.
pub enum AllowGlobalPackage {
    No,
    Yes,
}

impl From<bool> for AllowGlobalPackage {
    fn from(value: bool) -> Self {
        match value {
            true => AllowGlobalPackage::Yes,
            false => AllowGlobalPackage::No,
        }
    }
}

impl AllowGlobalPackage {
    pub fn allows(&self) -> bool {
        matches!(self, AllowGlobalPackage::Yes)
    }
}

// Disallow creating PackageLoader instances.
/// A loader of packages from the disk.
pub struct PackageLoader(PhantomData<()>);

impl PackageLoader {
    pub const MANIFEST_NAME: &str = "quackconfig.yaml";
    pub const FREEZE_NAME: &str = "quackfreeze.json";
    pub const VENV_CONFIG_NAME: &str = "venvconfig.yaml";

    /// Get the global package.
    pub fn global_package<'duck>(ctx: &'duck DuckContext) -> QuackResult<PackageContext<'duck>> {
        let global_package_path = ctx.duck_home().ensure_and_populate_global_dir()?;
        let global_package = PackageContext::new(global_package_path.to_path_buf(), ctx)?;
        if !global_package.is_global() {
            qp_bail!(
                "Global package should be named {}",
                DuckHome::GLOBAL_PACKAGE_NAME
            );
        } else {
            Ok(global_package)
        }
    }

    /// Find a [`PackageCtx`] from the given `start`.
    ///
    /// This function __expands tildes__ and __resolves__ path fully.
    /// Also, it walks up the chain of path's ancestors.
    pub fn find_from_directory<'duck>(
        start: &Path,
        ctx: &'duck DuckContext,
        allow_global_package: AllowGlobalPackage,
    ) -> QuackResult<PackageContext<'duck>> {
        let start = start.expand_user()?.resolve()?;
        if !start.is_dir() {
            let err = qp_err!("the path `{}` is not a directory", start.display());
            return Err(io::Error::new(io::ErrorKind::NotADirectory, err).into());
        }
        let mut current: &Path = start.as_ref();
        for potential_location in start.ancestors() {
            let path = potential_location.join(Self::MANIFEST_NAME);
            trace!("checking the path `{}`", path.display());
            if path.is_file() {
                debug!("found a package at `{}`", path.display());
                return PackageContext::new_not_global(potential_location.to_path_buf(), ctx);
            }
            current = potential_location;
        }
        if allow_global_package.allows() {
            PackageLoader::global_package(ctx)
        } else {
            qp_bail!(
                "no manifest has been found from the `{}` to the `{}`",
                start.display(),
                current.display(),
            )
        }
    }

    /// Find package at a given directory.
    ///
    /// Unlike [`find_from_directory`](Self::find_from_directory) this function __does not__ walk up
    /// `path`'s ancestors.
    /// It also does not check if the venv name conflicts with the global venv name (it may be used to load the global venv).
    pub fn find_at_exact_directory<'duck>(
        path: &Path,
        ctx: &'duck DuckContext,
    ) -> QuackResult<PackageContext<'duck>> {
        if !path.is_dir() {
            let err = qp_err!("the path `{}` is not a directory", path.display());
            return Err(io::Error::new(io::ErrorKind::NotADirectory, err).into());
        }
        let manifest_path = path.join(PackageLoader::MANIFEST_NAME);
        if !manifest_path.is_file() {
            let err = qp_err!("the directory `{}` has no manifest", path.display());
            return Err(io::Error::new(io::ErrorKind::NotFound, err).into());
        }
        PackageContext::new(path.to_path_buf(), ctx)
    }

    /// A convenient helper.
    pub fn find_from_cwd<'duck>(
        ctx: &'duck DuckContext,
        allow_global_package: AllowGlobalPackage,
    ) -> QuackResult<PackageContext<'duck>> {
        let cwd = ctx.cwd();
        Self::find_from_directory(cwd, ctx, allow_global_package)
    }

    /// Find the root of the venv with the given name.
    pub fn find_venv_by_name<'duck>(
        ctx: &'duck DuckContext,
        venv_id: VenvId,
    ) -> QuackResult<PackageContext<'duck>> {
        let storage_loc = ctx.default_storage_root();
        let storage = Storage::new(storage_loc);
        let Some(venv) = Venv::fix_and_load(&storage, venv_id)? else {
            qp_bail!("Could not find venv {} in the main storage", venv_id);
        };
        let dir = venv.data().last_known_directory();
        Self::find_at_exact_directory(dir, ctx)
            .with_context(|| format!("Lost track of the venv {venv_id}"))
    }
}

#[cfg(test)]
mod tests {
    use tempfile::tempdir;

    use crate::{
        DuckContext,
        quackpack::core::PackageLoader,
        util::path_ops_ext::{MkdirOptions, PathOpsExt},
    };

    const BASIC_MANIFEST: &str = r"
metadata:
  name: foo
  version: 0.1
";

    #[test]
    fn no_package_from_directory() {
        let tmp_file = tempdir().unwrap();
        let ctx = DuckContext::default();
        let err =
            PackageLoader::find_from_directory(tmp_file.path(), &ctx, false.into()).unwrap_err();
        assert_eq!(
            format!("{err}"),
            format!(
                "no manifest has been found from the `{}` to the `/`",
                tmp_file.path().resolve().unwrap().display()
            )
        );
    }

    #[test]
    fn not_a_dir() {
        let tmp_file = tempdir().unwrap();
        let file = tmp_file.path().join("x");
        let ctx = DuckContext::default();
        let err = PackageLoader::find_from_directory(&file, &ctx, false.into()).unwrap_err();
        assert_eq!(
            format!("{err}"),
            format!(
                "the path `{}` is not a directory",
                file.resolve().unwrap().display()
            )
        );
    }

    #[test]
    fn finds_from_directory() {
        let tmp_file = tempdir().unwrap();
        let file = tmp_file.path().join(PackageLoader::MANIFEST_NAME);
        file.touch().unwrap();
        file.write(BASIC_MANIFEST).unwrap();
        let ctx = DuckContext::default();
        let package =
            PackageLoader::find_from_directory(tmp_file.path(), &ctx, false.into()).unwrap();
        assert_eq!(
            package.package().root_directory().resolve().unwrap(),
            tmp_file.path().resolve().unwrap()
        );
    }

    #[test]
    fn finds_at_parent() {
        let tmp_file = tempdir().unwrap();
        let file = tmp_file.path().join(PackageLoader::MANIFEST_NAME);
        file.touch().unwrap();
        file.write(BASIC_MANIFEST).unwrap();
        let child = tmp_file.path().join("foo");
        child.mkdir(MkdirOptions::WithoutParents).unwrap();
        assert!(child.is_dir());
        let ctx = DuckContext::default();
        let package = PackageLoader::find_from_directory(&child, &ctx, false.into()).unwrap();
        assert_eq!(
            package.package().root_directory().resolve().unwrap(),
            tmp_file.path().resolve().unwrap()
        );
    }

    #[test]
    fn finds_at_exact_directory() {
        let tmp_file = tempdir().unwrap();
        let file = tmp_file.path().join(PackageLoader::MANIFEST_NAME);
        file.touch().unwrap();
        file.write(BASIC_MANIFEST).unwrap();
        let ctx = DuckContext::default();
        let package = PackageLoader::find_at_exact_directory(tmp_file.path(), &ctx).unwrap();
        assert_eq!(
            package.package().root_directory().resolve().unwrap(),
            tmp_file.path().resolve().unwrap()
        );
    }

    #[test]
    fn finds_at_exact_directory_notadir() {
        let tmp_file = tempdir().unwrap();
        let file = tmp_file.path().join("xd");
        assert!(!file.exists());
        let ctx = DuckContext::default();
        let err = PackageLoader::find_at_exact_directory(&file, &ctx).unwrap_err();
        assert_eq!(
            format!("{err}"),
            format!("the path `{}` is not a directory", file.display())
        );

        file.touch().unwrap();
        assert!(!file.is_dir());

        let err = PackageLoader::find_at_exact_directory(&file, &ctx).unwrap_err();
        assert_eq!(
            format!("{err}"),
            format!("the path `{}` is not a directory", file.display())
        );
    }
}
