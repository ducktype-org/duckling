//! General file lock support in QuackPack.
use std::fs::{File, OpenOptions, TryLockError};
use std::io::{self, Read, Seek, Write};
use std::path::{Display, Path, PathBuf};

use tracing::{debug, trace};

use crate::util::path_ops_ext::{MkdirOptions, PathOpsExt};
use crate::{DuckContext, QuackError, QuackResult, QuackResultContext};

#[derive(Debug)]
/// A locked [`File`], with its path.
///
/// Can be created via [`FileLockManager`] methods.
pub struct LockedFile {
    file: File,
    path: PathBuf,
}

impl LockedFile {
    /// Get the [`Path`] to the locked file.
    pub fn path(&self) -> &Path {
        self.path.as_path()
    }

    /// Get the locked [`File`].
    pub fn file(&self) -> &File {
        &self.file
    }

    /// Get the mutable reference to the locked [`File`].
    pub fn file_mut(&mut self) -> &mut File {
        &mut self.file
    }
}

impl Read for LockedFile {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.file.read(buf)
    }
}

impl Write for LockedFile {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.file.write(buf)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.file.flush()
    }
}

impl Seek for LockedFile {
    fn seek(&mut self, pos: io::SeekFrom) -> io::Result<u64> {
        self.file.seek(pos)
    }
}

impl Drop for LockedFile {
    fn drop(&mut self) {
        if let Err(e) = self.file.unlock() {
            debug!(
                "failed to unlock the file `{}`: {e} ({e:?})",
                self.path.display(),
            );
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
/// A hard to misuse locking wrapper around [`PathBuf`].
///
/// Generally provides only access to files via locks.
pub struct FileLockManager {
    root: PathBuf,
}

impl FileLockManager {
    /// Create a new [`FileLockManager`], rooted at `root`.
    pub fn new(root: PathBuf) -> Self {
        Self { root }
    }

    /// A wrapper around [`Path::join`].
    #[must_use = "returns a new `FileLockManager` without modifying the original"]
    pub fn join<P: AsRef<Path>>(&self, path: P) -> Self {
        Self {
            root: self.root.join(path),
        }
    }

    /// A wrapper around [`PathBuf::push`].
    pub fn push<P: AsRef<Path>>(&mut self, path: P) {
        self.root.push(path);
    }

    /// Get a [`Display`](std::fmt::Display) wrapper
    pub fn display(&self) -> Display<'_> {
        self.root.display()
    }

    /// Create recursive directories at this `root`.
    pub fn mkdir(&self) -> QuackResult<()> {
        self.root.mkdir(MkdirOptions::WithParents)
    }

    /// Open a `path` from `root`, in RW mode, create it, and lock it exclusively.
    pub fn open_exclusive(
        &self,
        path: impl AsRef<Path>,
        ctx: &DuckContext,
    ) -> QuackResult<LockedFile> {
        let path = self.root.join(path);
        let mut opts = OpenOptions::new();
        opts.create(true).read(true).write(true);
        let file = Self::open(&path, opts, true)?;
        lock(ctx, &path, || file.try_lock(), || file.lock())?;
        Ok(LockedFile { file, path })
    }

    /// Open a `path` from `root`, in RW mode, create it and try to lock it exclusively.
    ///
    /// `Ok(None)` is returned, if acquiring a lock would block.
    pub fn try_open_exclusive(&self, path: impl AsRef<Path>) -> QuackResult<Option<LockedFile>> {
        let path = self.root.join(path);
        let mut opts = OpenOptions::new();
        opts.create(true).read(true).write(true);
        let file = Self::open(&path, opts, true)?;
        if try_lock(&path, || file.try_lock())? {
            Ok(Some(LockedFile { file, path }))
        } else {
            Ok(None)
        }
    }

    /// Open a `path` from `root`, in RO mode, and shared lock it.
    ///
    /// This function will fail, if a file does not exist.
    pub fn open_existing_shared(
        &self,
        path: impl AsRef<Path>,
        ctx: &DuckContext,
    ) -> QuackResult<LockedFile> {
        let path = self.root.join(path);
        let mut opts = OpenOptions::new();
        opts.read(true);
        let file = Self::open(&path, opts, false)?;
        lock(ctx, &path, || file.try_lock_shared(), || file.lock_shared())?;
        Ok(LockedFile { file, path })
    }

    /// Open a `path` from `root`, in RO mode, and shared lock it.
    ///
    /// `Ok(None)` is returned, if acquiring a lock would block.
    ///
    /// This function will fail, if a file does not exist.
    pub fn try_open_existing_shared(
        &self,
        path: impl AsRef<Path>,
    ) -> QuackResult<Option<LockedFile>> {
        let path = self.root.join(path);
        let mut opts = OpenOptions::new();
        opts.read(true);
        let file = Self::open(&path, opts, false)?;
        if try_lock(&path, || file.try_lock_shared())? {
            Ok(Some(LockedFile { file, path }))
        } else {
            Ok(None)
        }
    }

    /// Open a `path` from `root`, in RW mode, create it, and shared lock it.
    ///
    /// Unfortunately, creating a file requires a write access.
    pub fn open_shared_rw(
        &self,
        path: impl AsRef<Path>,
        ctx: &DuckContext,
    ) -> QuackResult<LockedFile> {
        let path = self.root.join(path);
        let mut opts = OpenOptions::new();
        opts.read(true).write(true).create(true);
        let file = Self::open(&path, opts, true)?;
        lock(ctx, &path, || file.try_lock_shared(), || file.lock_shared())?;
        Ok(LockedFile { file, path })
    }

    /// Open a `path` from `root`, in RW mode, create it, and shared lock it.
    ///
    /// `Ok(None)` is returned, if acquiring a lock would block.
    ///
    /// Unfortunately, creating a file requires a write access.
    pub fn try_open_shared_rw(&self, path: impl AsRef<Path>) -> QuackResult<Option<LockedFile>> {
        let path = self.root.join(path);
        let mut opts = OpenOptions::new();
        opts.read(true).write(true).create(true);
        let file = Self::open(&path, opts, true)?;
        if try_lock(&path, || file.try_lock_shared())? {
            Ok(Some(LockedFile { file, path }))
        } else {
            Ok(None)
        }
    }

    /// A common helper for opening a file with options.
    fn open(path: &Path, opts: OpenOptions, create: bool) -> QuackResult<File> {
        opts.open(path)
            .or_else(|err| {
                if err.kind() == io::ErrorKind::NotFound && create {
                    if let Some(parent) = path.parent() {
                        parent.mkdir(MkdirOptions::WithParents)?;
                    }
                    Ok(opts.open(path)?)
                } else {
                    Err(QuackError::from(err))
                }
            })
            .with_context(|| format!("failed to open `{}`", path.display()))
    }

    /// Get a not locked [`Path`] to the root.
    pub fn not_locked_path(&self) -> &Path {
        &self.root
    }

    /// Convert into a not locked [`PathBuf`] to the root.
    pub fn into_not_locked_path(self) -> PathBuf {
        self.root
    }
}

/// Try to acquire a non-blocking lock.
fn try_lock(path: &Path, f: impl FnOnce() -> Result<(), TryLockError>) -> QuackResult<bool> {
    trace!(path = %path.display(), "trying to lock");
    match f() {
        Ok(()) => Ok(true),
        Err(TryLockError::WouldBlock) => Ok(false),
        Err(TryLockError::Error(e)) => {
            Err(e).context(format!("failed to lock `{}`", path.display()))
        }
    }
}

/// Try to acquire a blocking lock, but firstly try a non-blocking, and print a message.
fn lock(
    ctx: &DuckContext,
    path: &Path,
    non_blocking: impl FnOnce() -> Result<(), TryLockError>,
    blocking: impl FnOnce() -> io::Result<()>,
) -> QuackResult<()> {
    if try_lock(path, non_blocking)? {
        trace!(path = %path.display(), "locked nonblocking");
        return Ok(());
    }
    trace!(path = %path.display(), "locking blocking");
    ctx.console()
        .info(format!("waiting for file lock `{}`", path.display()))?;
    blocking().with_context(|| format!("failed to lock `{}`", path.display()))
}
