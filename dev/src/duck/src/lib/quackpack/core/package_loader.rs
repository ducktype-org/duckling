//! Loading packages from the disk.
use std::{io, marker::PhantomData, path::Path};

use tracing::{debug, trace};

use crate::{
    DuckCtx, QuackResult, qp_bail, qp_err, qp_internal, quackpack::core::PackageCtx,
    util_common::path_ops_ext::PathOpsExt,
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
    pub const MANIFEST_NAME: &str = "quackconfig.yml";
    pub const FREEZE_NAME: &str = "quackfreeze.json";
    pub const VENV_CONFIG_NAME: &str = "venvconfig.toml";

    /// Get the global package.
    pub fn global_package<'duck>(_ctx: &'duck DuckCtx) -> QuackResult<PackageCtx<'duck>> {
        Err(qp_internal!("@TODO: #1394 it needs the EditableManifest"))
    }

    /// Find a [`PackageCtx`] from a given `start`.
    ///
    /// This function __expands tildes__ and __resolves__ path fully.
    /// Also, it walks up the chain of path's ancestors.
    pub fn find_from_directory<'duck>(
        start: &Path,
        ctx: &'duck DuckCtx,
        allow_global_package: AllowGlobalPackage,
    ) -> QuackResult<PackageCtx<'duck>> {
        let start = start.expand_user()?.resolve()?;
        if !start.is_dir() {
            qp_bail!("the path `{}` is not a directory", start.display())
        }
        let mut current: &Path = start.as_ref();
        for potential_location in start.ancestors() {
            let path = potential_location.join(Self::MANIFEST_NAME);
            trace!("checking the path `{}`", path.display());
            if path.is_file() {
                debug!("found a package at `{}`", path.display());
                return PackageCtx::new(potential_location.to_path_buf(), ctx);
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
    pub fn find_at_exact_directory<'duck>(
        path: &Path,
        ctx: &'duck DuckCtx,
    ) -> QuackResult<PackageCtx<'duck>> {
        if !path.is_dir() {
            let err = qp_err!("the path `{}` is not a directory", path.display());
            return Err(io::Error::new(io::ErrorKind::NotADirectory, err).into());
        }
        let manifest_path = path.join(PackageLoader::MANIFEST_NAME);
        if !manifest_path.is_file() {
            let err = qp_err!("the directory `{}` has no manifest", path.display());
            return Err(io::Error::new(io::ErrorKind::NotFound, err).into());
        }
        PackageCtx::new(path.to_path_buf(), ctx)
    }

    /// A convenient helper.
    pub fn find_from_cwd<'duck>(
        ctx: &'duck DuckCtx,
        allow_global_package: AllowGlobalPackage,
    ) -> QuackResult<PackageCtx<'duck>> {
        let cwd = ctx.cwd();
        Self::find_from_directory(cwd, ctx, allow_global_package)
    }
}

#[cfg(test)]
mod tests {
    use tempfile::tempdir;

    use crate::{
        DuckCtx,
        quackpack::core::PackageLoader,
        util_common::path_ops_ext::{MkdirOptions, PathOpsExt},
    };

    const BASIC_MANIFEST: &str = r"
metadata:
  name: foo
  version: 0.1
";

    #[test]
    fn no_package_from_directory() {
        let tmp_file = tempdir().unwrap();
        let ctx = DuckCtx::default();
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
        let ctx = DuckCtx::default();
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
    fn founds_from_directory() {
        let tmp_file = tempdir().unwrap();
        let file = tmp_file.path().join(PackageLoader::MANIFEST_NAME);
        file.touch().unwrap();
        file.write(BASIC_MANIFEST).unwrap();
        let ctx = DuckCtx::default();
        let package =
            PackageLoader::find_from_directory(tmp_file.path(), &ctx, false.into()).unwrap();
        assert_eq!(
            package.package().root_directory().resolve().unwrap(),
            tmp_file.path().resolve().unwrap()
        );
    }

    #[test]
    fn founds_at_parent() {
        let tmp_file = tempdir().unwrap();
        let file = tmp_file.path().join(PackageLoader::MANIFEST_NAME);
        file.touch().unwrap();
        file.write(BASIC_MANIFEST).unwrap();
        let child = tmp_file.path().join("foo");
        child.mkdir(MkdirOptions::WithoutParents).unwrap();
        assert!(child.is_dir());
        let ctx = DuckCtx::default();
        let package = PackageLoader::find_from_directory(&child, &ctx, false.into()).unwrap();
        assert_eq!(
            package.package().root_directory().resolve().unwrap(),
            tmp_file.path().resolve().unwrap()
        );
    }

    #[test]
    fn founds_at_exact_directory() {
        let tmp_file = tempdir().unwrap();
        let file = tmp_file.path().join(PackageLoader::MANIFEST_NAME);
        file.touch().unwrap();
        file.write(BASIC_MANIFEST).unwrap();
        let ctx = DuckCtx::default();
        let package = PackageLoader::find_at_exact_directory(tmp_file.path(), &ctx).unwrap();
        assert_eq!(
            package.package().root_directory().resolve().unwrap(),
            tmp_file.path().resolve().unwrap()
        );
    }

    #[test]
    fn founds_at_exact_directory_notadir() {
        let tmp_file = tempdir().unwrap();
        let file = tmp_file.path().join("xd");
        assert!(!file.exists());
        let ctx = DuckCtx::default();
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
