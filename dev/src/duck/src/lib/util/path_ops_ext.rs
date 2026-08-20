use std::collections::VecDeque;
use std::ffi::OsStr;
use std::fs::{
    File, OpenOptions, Permissions, copy, create_dir, create_dir_all, hard_link, read,
    read_to_string, remove_dir, remove_file, rename, write,
};
use std::io::{self, Read, Write};
use std::path::{Component, Path, PathBuf};

use tracing::warn;

use crate::{DuckContext, QuackResult, QuackResultContext};

const BUFFER_SIZE: usize = 4096;

/// Options for controlling the [`PathOpsExt::mkdir`]
#[derive(Debug, Clone, Copy, Hash, Eq, PartialEq)]
pub enum MkdirOptions {
    /// Equivalent of the `mkdir $path`.
    WithoutParents,
    /// Equivalent of the `mkdir -p $path`.
    WithParents,
}

pub trait PathOpsExt {
    /// Copy the contents of one file into another and synchronize the result to disk.
    fn copy_file_to<P: AsRef<Path>>(&self, to: P) -> QuackResult<()>;

    /// Ensure that directory-level changes (creation, deletion, renaming) are persisted to disk.
    ///
    /// This is a best-effort operation. If unsupported, it will be silently skipped.
    fn try_fsync_dir(&self) -> QuackResult<()>;

    /// Touch the file and its parent directories.
    ///
    /// ## Returns
    /// [`Ok(File)`](std::fs::File) if created successfully, otherwise an error, as reported by
    /// the [`PathOpsExt::mkdir`] or the [`OpenOptions::open`].
    fn touch(&self) -> QuackResult<File>;

    /// Create directories at given [`Path`].
    ///
    /// ## Returns
    /// [`Ok(())`](Ok) if created successfully, otherwise an error, as reported by
    /// the [`create_dir`], or the [`create_dir_all`].
    ///
    /// Note that this function will return `Ok(())`, if [`create_dir`] returns `Err` with kind
    /// [`ErrorKind::AlreadyExists`](io::ErrorKind::AlreadyExists).
    fn mkdir(&self, opts: MkdirOptions) -> QuackResult<()>;

    /// Normalize `self` fully, as best as possible.
    ///
    /// __WARNING!__: this function does __not__ care for CWD. You should firstly manually join it,
    /// like: `ctx.cwd().join(path).normalize();`.
    ///
    /// Unlike [`std::fs::canonicalize`], this function __doesn't__ fail, if `self` points to a
    /// non-existing file.
    fn normalize(&self) -> PathBuf;

    /// Normalize `self` fully, against `ctx.cwd()`.
    ///
    /// This is equivalent to:
    /// ```rust,ignore (illustrative)
    /// ctx.cwd().join(self).normalize()
    /// ```
    fn resolve(&self, ctx: &DuckContext) -> PathBuf;

    /// Normalize expanded `self` fully, against `ctx.cwd()`.
    ///
    /// This is equivalent to:
    /// ```rust,ignore (illustrative)
    /// ctx.cwd().join(self.expand_tilde(ctx)).normalize()
    /// ```
    fn resolve_with_tilde(&self, ctx: &DuckContext) -> PathBuf;

    /// Expand `~` into the `ctx.user_home()`.
    fn expand_tilde(&self, ctx: &DuckContext) -> PathBuf;

    /// Expand `~` into a `home`.
    fn expand_tilde_with(&self, home: &Path) -> PathBuf;

    /// Resolves both `self` and `other` and returns the relative path from `self` to `other`.
    fn resolve_both_and_get_relative(&self, other: &Path, ctx: &DuckContext) -> PathBuf;

    /// Returns `true` if path exists on a disk and points to an executable file.
    ///
    /// Current implementation only considers `unix` and `windows` cfg's, any other always returns
    /// `false`.
    fn is_executable(&self) -> bool;

    /// A wrapper around [`std::fs::copy`].
    fn copy_to(&self, to: impl AsRef<Path>) -> QuackResult<u64>;

    /// A wrapper around [`std::fs::hard_link`].
    fn hard_link_to(&self, to: impl AsRef<Path>) -> QuackResult<()>;

    /// A wrapper around [`std::fs::read`].
    fn read(&self) -> QuackResult<Vec<u8>>;

    /// A wrapper around [`std::fs::read_to_string`].
    fn read_to_string(&self) -> QuackResult<String>;

    /// A wrapper around [`std::fs::rename`].
    fn rename_to(&self, to: impl AsRef<Path>) -> QuackResult<()>;

    /// A wrapper around [`std::fs::remove_file`].
    fn rm(&self) -> QuackResult<()>;

    /// A wrapper around [`std::fs::remove_dir`].
    fn rmdir(&self) -> QuackResult<()>;

    /// A wrapper around [`std::fs::remove_dir_all`].
    fn rmtree(&self) -> QuackResult<()>;

    /// A wrapper around [`std::fs::set_permissions`].
    fn set_permissions(&self, permissions: Permissions) -> QuackResult<()>;

    /// A wrapper around [`std::fs::write`].
    fn write(&self, contents: impl AsRef<[u8]>) -> QuackResult<()>;
}

impl PathOpsExt for Path {
    fn copy_file_to<P: AsRef<Path>>(&self, to: P) -> QuackResult<()> {
        let mut source = {
            let mut opts = OpenOptions::new();
            opts.read(true).open(self)
        }
        .with_context(|| format!("failed to open `{}` readonly", self.display()))?;
        let to = to.as_ref();
        let mut target = {
            let mut opts = OpenOptions::new();
            opts.write(true).create(true).truncate(true).open(to)
        }
        .with_context(|| format!("failed to open `{}` write-only", to.display()))?;
        let mut buffer = [0; BUFFER_SIZE];
        loop {
            let n = source
                .read(&mut buffer)
                .with_context(|| format!("failed to read from `{}`", self.display()))?;
            if n == 0 {
                break;
            }
            target
                .write_all(&buffer[..n])
                .with_context(|| format!("failed to write to `{}`", to.display()))?;
        }
        target
            .flush()
            .with_context(|| format!("failed to flush `{}`", to.display()))?;
        target
            .sync_data()
            .with_context(|| format!("failed to sync `{}`", to.display()))?;
        Ok(())
    }

    #[cfg(not(windows))]
    fn try_fsync_dir(&self) -> QuackResult<()> {
        let dir = {
            let mut opts = OpenOptions::new();
            opts.read(true).open(self)
        }
        .with_context(|| format!("failed to open `{}` readonly", self.display()))?;
        match dir.sync_data() {
            Ok(_) => Ok(()),
            Err(e) if matches!(e.kind(), io::ErrorKind::Unsupported) => Ok(()),
            Err(e) => Err(e).with_context(|| format!("failed to sync `{}", self.display())),
        }
    }

    #[cfg(windows)]
    fn try_fsync_dir(&self) -> QuackResult<()> {
        // sync_data and sync_all are unreliable on Windows and usually unneeded.
        Ok(())
    }

    fn touch(&self) -> QuackResult<File> {
        if let Some(parent) = self.parent() {
            parent.mkdir(MkdirOptions::WithParents)?;
        }
        let mut opts = OpenOptions::new();
        opts.read(true).write(true).create(true).truncate(false);
        #[cfg(unix)]
        {
            use std::os::unix::fs::{OpenOptionsExt, PermissionsExt};
            if let Ok(metadata) = self.metadata() {
                // RDWR for all.
                const MASK: u32 = 0o666;
                let permissions = metadata.permissions().mode();
                opts.mode(permissions & MASK);
            }
        }
        opts.open(self)
            .with_context(|| format!("failed to create file `{}`", self.display()))
    }

    fn mkdir(&self, opts: MkdirOptions) -> QuackResult<()> {
        let result = match opts {
            MkdirOptions::WithoutParents => create_dir(self),
            MkdirOptions::WithParents => create_dir_all(self),
        };
        let text = match opts {
            MkdirOptions::WithoutParents => "directory",
            MkdirOptions::WithParents => "directory with parents",
        };
        match result {
            Err(e) if e.kind() == io::ErrorKind::AlreadyExists => Ok(()),
            _ => result,
        }
        .with_context(|| format!("failed to create {text} `{}`", self.display()))
    }

    #[cfg(unix)]
    fn is_executable(&self) -> bool {
        use std::os::unix::prelude::*;
        self.metadata()
            .map(|metadata| {
                // Note: This should be the same as 0o111.
                #[allow(clippy::unnecessary_cast)] // On macOS those are u16, on Linux they are u32.
                const EXEC_MASK: u32 = (libc::S_IXUSR | libc::S_IXGRP | libc::S_IXOTH) as u32;
                const _: () = assert!(EXEC_MASK == 0o111, "bits mismatch");
                metadata.is_file() && metadata.permissions().mode() & EXEC_MASK != 0
            })
            .unwrap_or(false)
    }

    #[cfg(windows)]
    fn is_executable(&self) -> bool {
        self.is_file()
    }

    #[cfg(not(any(unix, windows)))]
    fn is_executable(&self) -> bool {
        false
    }

    fn copy_to(&self, to: impl AsRef<Path>) -> QuackResult<u64> {
        let to = to.as_ref();
        copy(self, to)
            .with_context(|| format!("failed to copy `{}` to `{}", self.display(), to.display()))
    }

    fn hard_link_to(&self, to: impl AsRef<Path>) -> QuackResult<()> {
        let to = to.as_ref();
        hard_link(self, to).with_context(|| {
            format!(
                "failed to hard link `{}` to `{}",
                self.display(),
                to.display()
            )
        })
    }

    fn read(&self) -> QuackResult<Vec<u8>> {
        read(self).with_context(|| format!("failed to read `{}`", self.display()))
    }

    fn read_to_string(&self) -> QuackResult<String> {
        read_to_string(self).with_context(|| format!("failed to read `{}`", self.display()))
    }

    fn rename_to(&self, to: impl AsRef<Path>) -> QuackResult<()> {
        let to = to.as_ref();
        rename(self, to).with_context(|| {
            format!(
                "failed to rename `{}` to `{}`",
                self.display(),
                to.display()
            )
        })
    }

    fn rm(&self) -> QuackResult<()> {
        ignore_not_found(remove_file(self))
            .with_context(|| format!("failed to remove file `{}`", self.display()))
    }

    fn rmdir(&self) -> QuackResult<()> {
        ignore_not_found(remove_dir(self))
            .with_context(|| format!("failed to remove directory `{}`", self.display()))
    }

    fn rmtree(&self) -> QuackResult<()> {
        ignore_not_found(std::fs::remove_dir_all(self))
            .with_context(|| format!("failed to remove tree at `{}`", self.display()))
    }

    fn set_permissions(&self, permissions: Permissions) -> QuackResult<()> {
        std::fs::set_permissions(self, permissions)
            .with_context(|| format!("failed to set permissions on `{}`", self.display()))
    }

    fn write(&self, contents: impl AsRef<[u8]>) -> QuackResult<()> {
        write(self, contents.as_ref())
            .with_context(|| format!("failed to write to `{}`", self.display()))
    }

    #[track_caller]
    fn normalize(&self) -> PathBuf {
        let mut result: Vec<Component> = vec![];
        let components = self.components().peekable();

        for component in components {
            match component {
                pref @ Component::Prefix(..) => result.push(pref),
                root @ Component::RootDir => result.push(root),
                normal @ Component::Normal(..) => result.push(normal),
                parent @ Component::ParentDir => match result.last() {
                    Some(Component::Prefix(_)) => {}
                    Some(Component::RootDir) => {}
                    Some(Component::ParentDir) => result.push(parent),
                    Some(Component::Normal(_)) => {
                        result.pop();
                    }
                    None => result.push(parent),
                    Some(Component::CurDir) => {
                        unreachable!("we removed all curdirs but the current path is `{result:?}`");
                    }
                },
                Component::CurDir => {}
            }
        }
        if result.is_empty() {
            result.push(Component::CurDir);
        }
        PathBuf::from_iter(result)
    }

    fn expand_tilde(&self, ctx: &DuckContext) -> PathBuf {
        self.expand_tilde_with(ctx.user_home())
    }

    fn expand_tilde_with(&self, home: &Path) -> PathBuf {
        match self.strip_prefix("~") {
            Ok(rest) => home.join(rest),
            Err(_) => {
                let os_str = self.as_os_str();
                let tilde = OsStr::new("~");
                if os_str
                    .as_encoded_bytes()
                    .starts_with(tilde.as_encoded_bytes())
                {
                    warn!(path = ?self, "path (likely) contains a shell syntax for a different user's home directory (like `~user`), which is not supported");
                }
                self.to_path_buf()
            }
        }
    }

    fn resolve(&self, ctx: &DuckContext) -> PathBuf {
        ctx.cwd().join(self).normalize()
    }

    fn resolve_with_tilde(&self, ctx: &DuckContext) -> PathBuf {
        ctx.cwd().join(self.expand_tilde(ctx)).normalize()
    }

    fn resolve_both_and_get_relative(&self, other: &Path, ctx: &DuckContext) -> PathBuf {
        let source = self.resolve_with_tilde(ctx);
        let target = other.resolve_with_tilde(ctx);
        let mut src_components = source.components();
        let mut tgt_components = target.components();

        let mut result_components = VecDeque::new();
        let mut common_prefix = true;
        loop {
            match (src_components.next(), tgt_components.next()) {
                (None, None) => break,
                (Some(src_comp), Some(tgt_comp)) => {
                    if src_comp == tgt_comp && common_prefix {
                        continue;
                    }
                    common_prefix = false;
                    result_components.push_front(Component::ParentDir);
                    result_components.push_back(tgt_comp);
                }
                (Some(_), None) => {
                    common_prefix = false;
                    result_components.push_front(Component::ParentDir)
                }
                (None, Some(tgt_comp)) => {
                    common_prefix = false;
                    result_components.push_back(tgt_comp)
                }
            }
        }
        result_components
            .into_iter()
            .map(|c| c.as_os_str())
            .collect()
    }
}

fn ignore_io_kind_error<T: Default>(
    err: io::Result<T>,
    to_ignore: &[io::ErrorKind],
) -> io::Result<T> {
    match err {
        x @ Ok(_) => x,
        Err(e) => {
            if to_ignore.contains(&e.kind()) {
                Ok(T::default())
            } else {
                Err(e)
            }
        }
    }
}

fn ignore_not_found<T: Default>(err: io::Result<T>) -> io::Result<T> {
    ignore_io_kind_error(err, &[io::ErrorKind::NotFound])
}

#[cfg(test)]
mod tests {
    use std::path::Path;

    use super::*;

    #[test]
    fn resolve_tests() {
        let path = Path::new("/../../../cfg");
        assert_eq!(path.normalize(), Path::new("/cfg"));

        let path = Path::new("/../../..");
        assert_eq!(path.normalize(), Path::new("/"));

        let path = Path::new("/home/../.");
        assert_eq!(path.normalize(), Path::new("/"));

        let path = Path::new("/home/duckling/./xd");
        assert_eq!(path.normalize(), Path::new("/home/duckling/xd"));

        let path = Path::new("/home/duckling/./xd/..");
        assert_eq!(path.normalize(), Path::new("/home/duckling"));

        let path = Path::new("../a/b/c/../../../../d");
        assert_eq!(path.normalize(), Path::new("../../d"));
    }

    #[test]
    fn home_tests() {
        let home = Path::new("/home/duckling");
        let path = Path::new("~");
        assert_eq!(path.expand_tilde_with(home), home);

        let path = Path::new("~/");
        assert_eq!(path.expand_tilde_with(home), home);

        let path = Path::new("~/foo");
        assert_eq!(path.expand_tilde_with(home), home.join("foo"));

        let path = Path::new("~/foo/../bar");
        assert_eq!(
            path.expand_tilde_with(home),
            home.join("foo").join("..").join("bar")
        );

        let path = Path::new(".");
        assert_eq!(path.expand_tilde_with(home), path,);

        let path = Path::new("/duckling");
        assert_eq!(path.expand_tilde_with(home), path,);

        let path = Path::new("duckling/ducktype");
        assert_eq!(path.expand_tilde_with(home), path,);

        // Other users' syntax is not supported.
        let path = Path::new("~duck/");
        assert_eq!(path.expand_tilde_with(home), path,);

        let path = Path::new("~duck");
        assert_eq!(path.expand_tilde_with(home), path,);
    }

    #[test]
    fn relative_tests() {
        let ctx = DuckContext::default();

        let source = Path::new("foo/bar");
        let target = Path::new("x/y/z");
        let expected = Path::new("../../x/y/z");
        assert_eq!(source.resolve_both_and_get_relative(target, &ctx), expected);

        let source = Path::new("");
        let target = Path::new("x/y/z");
        let expected = Path::new("x/y/z");
        assert_eq!(source.resolve_both_and_get_relative(target, &ctx), expected);

        let source = Path::new("foo/x");
        let target = Path::new("foo/y");
        let expected = Path::new("../y");
        assert_eq!(source.resolve_both_and_get_relative(target, &ctx), expected);

        let source = Path::new("foo/bar");
        let target = Path::new("../x/../y/z");
        let expected = Path::new("../../../y/z");
        assert_eq!(source.resolve_both_and_get_relative(target, &ctx), expected);
    }
}
