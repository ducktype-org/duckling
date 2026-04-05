use std::{
    fs::{
        File, OpenOptions, Permissions, copy, create_dir, create_dir_all, hard_link, read,
        read_to_string, remove_dir, remove_file, rename, write,
    },
    io::{self, Read, Write},
    ops::{Deref, DerefMut},
    path::{Path, PathBuf},
};

use crate::{QuackResult, QuackResultContext, qp_bail};

const BUFFER_SIZE: usize = 4096;

/// Whether the [`PathOpsExt::lock`]/[`PathOpsExt::lock_shared`] should block the current thread.
#[derive(Debug, Hash, Clone, Copy, PartialEq, Eq)]
pub enum ShouldBlock {
    No,
    Yes,
}

/// An RAII guard, which calls [`(*self).unlock()`](std::fs::File::unlock) on a drop.
#[derive(Debug)]
pub struct FileLockGuard {
    file: File,
}

impl Drop for FileLockGuard {
    fn drop(&mut self) {
        drop(self.file.unlock())
    }
}

impl Deref for FileLockGuard {
    type Target = File;

    fn deref(&self) -> &Self::Target {
        &self.file
    }
}

impl DerefMut for FileLockGuard {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.file
    }
}

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

    /// Locks exclusively `self`, creating a file if needed.
    ///
    /// This is essentially [`self.touch()?`](PathOpsExt::touch) followed by [`File::lock`]/[`File::try_lock`], with RAII bloat.
    fn lock(&self, should_block: ShouldBlock) -> QuackResult<FileLockGuard>;

    /// Locks shared `self`, creating a file if needed.
    ///
    /// This is essentially [`self.touch()?`](PathOpsExt::touch) followed by [`File::lock_shared`]/[`File::try_lock_shared`], with RAII bloat.
    ///
    /// ## Returns
    /// [`Ok(FileLockGuard)`](FileLockGuard) on a success.
    fn lock_shared(&self, should_block: ShouldBlock) -> QuackResult<FileLockGuard>;

    /// Resolve `self` fully, as best as possible.
    ///
    /// Unlike [`std::fs::canonicalize`], this function __doesn't__ fail, if `self` points to a
    /// non-existing file.
    ///
    /// This function requires the __full-resolve__ feature.
    fn resolve(&self) -> QuackResult<PathBuf>;

    /// Canonicalize `self` fully: expand `~` into the `$HOME`.
    ///
    /// Also, because of the current implementation, this function will fail, if `self` is not a
    /// utf8 path.
    ///
    /// This function requires the __expand-user__ feature.
    fn expand_user(&self) -> QuackResult<PathBuf>;

    /// Canonicalize `self` fully: expand `~` into a `home`.
    ///
    /// Also, because of the current implementation, this function will fail, if `self` is not a
    /// ut8 path.
    ///
    /// This function requires the __expand-user__ feature.
    fn expand_user_with(&self, home: impl AsRef<str>) -> QuackResult<PathBuf>;

    /// Canonicalize `self` fully: expand `~` into a `home()`.
    ///
    /// Also, because of the current implementation, this function will fail, if `self` is not a
    /// utf8 path.
    ///
    /// This function requires the __expand-user__ feature.
    fn expand_user_with_fn<F, H>(&self, home: F) -> QuackResult<PathBuf>
    where
        H: AsRef<str>,
        F: FnOnce() -> H;

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
            opts.write(true).create(true).open(to)
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

    fn lock(&self, should_block: ShouldBlock) -> QuackResult<FileLockGuard> {
        let file = self.touch()?;
        let result = if matches!(should_block, ShouldBlock::Yes) {
            file.lock()
        } else {
            file.try_lock().map_err(|err| match err {
                std::fs::TryLockError::Error(error) => error,
                std::fs::TryLockError::WouldBlock => io::Error::from(io::ErrorKind::WouldBlock),
            })
        };
        result.map(|_| FileLockGuard { file }).with_context(|| {
            format!(
                "failed to acquire an exclusive lock on `{}`",
                self.display()
            )
        })
    }

    fn lock_shared(&self, should_block: ShouldBlock) -> QuackResult<FileLockGuard> {
        let file = self.touch()?;
        let result = if matches!(should_block, ShouldBlock::Yes) {
            file.lock_shared()
        } else {
            file.try_lock_shared().map_err(|err| match err {
                std::fs::TryLockError::Error(error) => error,
                std::fs::TryLockError::WouldBlock => io::Error::from(io::ErrorKind::WouldBlock),
            })
        };
        result
            .map(|_| FileLockGuard { file })
            .with_context(|| format!("failed to acquire a shared lock on `{}`", self.display()))
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
        remove_file(self).with_context(|| format!("failed to remove file `{}`", self.display()))
    }

    fn rmdir(&self) -> QuackResult<()> {
        remove_dir(self).with_context(|| format!("failed to remove directory `{}`", self.display()))
    }

    fn rmtree(&self) -> QuackResult<()> {
        std::fs::remove_dir_all(self)
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

    fn resolve(&self) -> QuackResult<PathBuf> {
        use soft_canonicalize::soft_canonicalize;
        soft_canonicalize(self).with_context(|| format!("failed to resolve `{}`", self.display()))
    }

    fn expand_user(&self) -> QuackResult<PathBuf> {
        use shellexpand::tilde;
        let Some(as_str) = self.to_str() else {
            qp_bail!("path `{}` is not a utf8 string", self.display())
        };
        Ok(PathBuf::from(tilde(as_str).into_owned()))
    }

    fn expand_user_with(&self, home: impl AsRef<str>) -> QuackResult<PathBuf> {
        self.expand_user_with_fn(|| home)
    }

    fn expand_user_with_fn<F, H>(&self, home: F) -> QuackResult<PathBuf>
    where
        H: AsRef<str>,
        F: FnOnce() -> H,
    {
        use shellexpand::tilde_with_context;
        let Some(as_str) = self.to_str() else {
            qp_bail!("path `{}` is not a utf8 string", self.display())
        };
        Ok(PathBuf::from(
            tilde_with_context(as_str, || Some(home())).into_owned(),
        ))
    }
}
