//! Loading packages from the disk.
use std::fmt::Display;
use std::io;
use std::marker::PhantomData;
use std::path::{Path, PathBuf};

use tracing::debug;

use crate::duck::util::duck_home::DuckHome;
use crate::quackpack::core::PackageContext;
use crate::quackpack::core::script::{PackageScript, Script};
use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::core::storage::venv::Venv;
use crate::quackpack::core::storage::venv_id::VenvId;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult, QuackResultContext, qp_bail, qp_err};

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
    pub fn allows(self) -> bool {
        matches!(self, AllowGlobalPackage::Yes)
    }
}

// Disallow creating PackageLoader instances.
/// A loader of packages from the disk.
pub struct PackageLoader(PhantomData<()>);

#[derive(Debug)]
/// Error signifying that PackageLoader did not find a package.
pub struct PackageNotFound {
    start_path: PathBuf,
    end_path: PathBuf,
}

impl PackageNotFound {
    /// Create a new [`PackageNotFound`], describing failure to find a package between `start` and `end`.
    fn new(start: &Path, end: &Path) -> Self {
        Self {
            start_path: start.to_path_buf(),
            end_path: end.to_path_buf(),
        }
    }
}

impl std::error::Error for PackageNotFound {}

impl PackageLoader {
    pub const MANIFEST_NAME: &str = "quackconfig.yaml";
    pub const FREEZE_NAME: &str = "quackfreeze.json";
    pub const VENV_CONFIG_NAME: &str = "venvconfig.yaml";

    /// Get the global package.
    pub fn global_package<'duck>(ctx: &'duck DuckContext) -> QuackResult<PackageContext<'duck>> {
        let global_package_path = ctx.duck_home().ensure_and_populate_global_dir()?;
        let global_package = PackageContext::new(global_package_path.into_not_locked_path(), ctx)?;
        if !global_package.is_global() {
            qp_bail!(
                "Global package should be named {}",
                DuckHome::GLOBAL_PACKAGE_NAME
            );
        } else {
            Ok(global_package)
        }
    }

    /// Find a [`PackageContext`] from the given `start`.
    ///
    /// This function __expands tildes__ and __resolves__ path fully.
    /// Also, it walks up the chain of path's ancestors.
    pub fn find_from_directory<'duck>(
        start: &Path,
        ctx: &'duck DuckContext,
        allow_global_package: AllowGlobalPackage,
    ) -> QuackResult<PackageContext<'duck>> {
        let start = start.resolve(ctx);
        if !start.is_dir() {
            let err = qp_err!("the path `{}` is not a directory", start.display());
            return Err(io::Error::new(io::ErrorKind::NotADirectory, err).into());
        }
        let mut current: &Path = start.as_ref();
        for potential_location in start.ancestors() {
            let path = potential_location.join(Self::MANIFEST_NAME);
            debug!(path = %path.display(), "checking for the manifest");
            if path.is_file() {
                debug!(found = %path.display(), "found a manifest");
                return PackageContext::new_not_global(potential_location.to_path_buf(), ctx);
            }
            current = potential_location;
        }
        if allow_global_package.allows() {
            PackageLoader::global_package(ctx)
        } else {
            qp_bail!(PackageNotFound::new(&start, current))
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
        let storage = Storage::new(storage_loc.into_not_locked_path());
        let Some(venv) = Venv::fix_and_load(&storage, venv_id, ctx)? else {
            qp_bail!("Could not find venv {} in the main storage", venv_id);
        };
        let dir = venv.data().last_known_location();
        Self::find_at_exact_directory(dir, ctx)
            .with_context(|| format!("Lost track of the venv {venv_id}"))
    }

    /// Loads the appropriate venv of the script.
    #[track_caller]
    pub fn load_script<'duck>(
        ctx: &'duck DuckContext,
        script_path: &Path,
        script_folder: &Path,
        global: bool,
    ) -> QuackResult<PackageContext<'duck>> {
        let associated_script = |package| {
            let script = PackageScript::new(package, script_path.to_path_buf())?;
            Ok(PackageContext::new_script(script.into(), ctx))
        };
        let global_package = PackageLoader::global_package(ctx)
            .context_internal("failed to load the global package")?
            .into_package()
            .unwrap_package();
        let has_frontmatter = Script::has_frontmatter(script_path)?;
        // If this is `Ok(_)` then the script lies inside a package.
        let possible_package =
            PackageLoader::find_from_directory(script_folder, ctx, AllowGlobalPackage::No);
        // If possible_package is `Err` and it does not steem from `PackageNotFound` then return the error.
        // After this, possible_package is `Err` if and only if script does not belong to a package.
        if let Err(ref err) = possible_package
            && !err.has_in_chain::<PackageNotFound>()
        {
            return possible_package;
        }
        match (has_frontmatter, possible_package, global) {
            // Scripts with frontmatters cannot be inside packages nor be run with `global` flag.
            (true, Ok(_), _) => qp_bail!("scripts inside packages cannot have frontmatters"),
            (true, _, true) => {
                qp_bail!("script with a frontmatter cannot be run with `global` flag")
            }
            (true, Err(_), false) => PackageContext::new_standalone_script(script_path, ctx),
            // `global` forces the script to be run in the global venv, even if it is inside a package.
            (false, _, true) => associated_script(global_package),
            // If script does not belong to a package, nor any special option has been specified,
            // treat it as a script under the global package (the default of defaults).
            (false, Err(_), false) => associated_script(global_package),
            // Script under a package.
            (false, Ok(pcx), false) => {
                let package = pcx.into_package().unwrap_package();
                associated_script(package)
            }
        }
    }
}

impl Display for PackageNotFound {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "no manifest has been found from the `{}` to the `{}`",
            self.start_path.display(),
            self.end_path.display()
        )
    }
}

#[cfg(test)]
mod tests {
    use tempfile::tempdir;

    use crate::DuckContext;
    use crate::quackpack::core::PackageLoader;
    use crate::util::path_ops_ext::{MkdirOptions, PathOpsExt};

    const BASIC_MANIFEST: &str = r"
metadata:
  name: foo
  version: '0.1'
";

    #[test]
    fn no_package_from_directory() {
        #[cfg(windows)]
        let root = "C:\\";
        #[cfg(not(windows))]
        let root = "/";
        let tmp_file = tempdir().unwrap();
        let ctx = DuckContext::default();
        let err =
            PackageLoader::find_from_directory(tmp_file.path(), &ctx, false.into()).unwrap_err();
        assert_eq!(
            format!("{err}"),
            format!(
                "no manifest has been found from the `{}` to the `{}`",
                tmp_file.path().normalize().display(),
                root
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
                file.normalize().display()
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
            package
                .into_package()
                .unwrap_package()
                .root_directory()
                .normalize(),
            tmp_file.path().normalize(),
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
            package
                .into_package()
                .unwrap_package()
                .root_directory()
                .normalize(),
            tmp_file.path().normalize()
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
            package
                .into_package()
                .unwrap_package()
                .root_directory()
                .normalize(),
            tmp_file.path().normalize()
        );
    }

    #[test] // cSpell:disable-next-line
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
