// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::collections::VecDeque;
use std::ffi::OsStr;
use std::fs::{self, File, OpenOptions, Permissions};
use std::io::{self, Read, Write};
use std::path::{Component, Path, PathBuf};
use std::time::SystemTime;

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

/// Copy the contents of one file into another and synchronize the result to disk.
pub fn copy_file(from: impl AsRef<Path>, to: impl AsRef<Path>) -> QuackResult<()> {
    let from = from.as_ref();
    let to = to.as_ref();
    copy_file_inner(from, to)
        .with_context(|| format!("failed to copy `{}` to `{}`", from.display(), to.display()))
}

fn copy_file_inner(from: &Path, to: &Path) -> io::Result<()> {
    let mut source = {
        let mut opts = OpenOptions::new();
        opts.read(true).open(from)?
    };
    let mut target = {
        let mut opts = OpenOptions::new();
        opts.write(true).create(true).truncate(true).open(to)?
    };
    let mut buffer = [0; BUFFER_SIZE];
    loop {
        let n = source.read(&mut buffer)?;
        if n == 0 {
            break;
        }
        target.write_all(&buffer[..n])?;
    }
    target.flush()?;
    target.sync_data()?;
    Ok(())
}

/// Ensure that directory-level changes (creation, deletion, renaming) are persisted to disk.
///
/// This is a best-effort operation. If unsupported, it will be silently skipped.
pub fn try_fsync_dir(dir: impl AsRef<Path>) -> QuackResult<()> {
    let dir = dir.as_ref();
    try_fsync_dir_inner(dir)
        .with_context(|| format!("failed to sync directory `{}`", dir.display()))
}

#[cfg(not(windows))]
fn try_fsync_dir_inner(dir: &Path) -> io::Result<()> {
    let file = {
        let mut opts = OpenOptions::new();
        opts.read(true).open(dir)?
    };
    match file.sync_data() {
        Ok(_) => Ok(()),
        Err(e) if matches!(e.kind(), io::ErrorKind::Unsupported) => Ok(()),
        Err(e) => Err(e),
    }
}

#[cfg(windows)]
fn try_fsync_dir_inner(_dir: &Path) -> io::Result<()> {
    // sync_data and sync_all are unreliable on Windows and usually unneeded.
    Ok(())
}

/// Touch the file and its parent directories.
///
/// ## Returns
/// [`Ok(File)`](std::fs::File) if created successfully, otherwise an error, as reported by
/// the [`PathOpsExt::mkdir`] or the [`OpenOptions::open`].
pub fn touch(path: impl AsRef<Path>) -> QuackResult<File> {
    let path = path.as_ref();
    touch_inner(path).with_context(|| format!("failed to touch `{}`", path.display()))
}

fn touch_inner(path: &Path) -> io::Result<File> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    let mut opts = OpenOptions::new();
    opts.read(true).write(true).create(true).truncate(false);
    #[cfg(unix)]
    {
        use std::os::unix::fs::{OpenOptionsExt, PermissionsExt};
        if let Ok(metadata) = fs::metadata(path) {
            // RDWR for all.
            const MASK: u32 = 0o666;
            let permissions = metadata.permissions().mode();
            opts.mode(permissions & MASK);
        }
    }
    opts.open(path)
}

/// Open a file with the given [`fs::OpenOptions`].
///
/// ## Returns
/// [`Ok(File)`](std::fs::File) if created successfully, otherwise an error, as reported by
/// the [`PathOpsExt::mkdir`] or the [`OpenOptions::open`].
pub fn open(path: impl AsRef<Path>, opts: &OpenOptions) -> QuackResult<File> {
    let path = path.as_ref();
    opts.open(path)
        .with_context(|| format!("failed to open `{}`", path.display()))
}

/// Create directories at given [`Path`].
///
/// ## Returns
/// [`Ok(())`](Ok) if created successfully, otherwise an error, as reported by
/// the [`create_dir`], or the [`create_dir_all`].
///
/// Note that this function will return `Ok(())`, if [`create_dir`] returns `Err` with kind
/// [`ErrorKind::AlreadyExists`](io::ErrorKind::AlreadyExists).
pub fn mkdir(path: impl AsRef<Path>, opts: MkdirOptions) -> QuackResult<()> {
    let path = path.as_ref();
    let result = match opts {
        MkdirOptions::WithoutParents => fs::create_dir(path),
        MkdirOptions::WithParents => fs::create_dir_all(path),
    };
    let text = match opts {
        MkdirOptions::WithoutParents => "directory",
        MkdirOptions::WithParents => "directory with parents",
    };
    match result {
        Err(e) if e.kind() == io::ErrorKind::AlreadyExists => Ok(()),
        _ => result,
    }
    .with_context(|| format!("failed to create {text} `{}`", path.display()))
}

/// Normalize `self` fully, as best as possible.
///
/// __WARNING!__: this function does __not__ care for CWD. You should firstly manually join it,
/// like: `ctx.cwd().join(path).normalize();`.
///
/// Unlike [`std::fs::canonicalize`], this function __doesn't__ fail, if `self` points to a
/// non-existing file.
#[track_caller]
pub fn normalize(path: impl AsRef<Path>) -> PathBuf {
    normalize_inner(path.as_ref())
}

// This is out of line, since it's pretty large.
fn normalize_inner(path: &Path) -> PathBuf {
    let mut result: Vec<Component> = vec![];
    let components = path.components().peekable();

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

/// Normalize `self` fully, against `ctx.cwd()`.
///
/// This is equivalent to:
/// ```rust,ignore (illustrative)
/// normalize(ctx.cwd().join(self))
/// ```
pub fn resolve(path: impl AsRef<Path>, ctx: &DuckContext) -> PathBuf {
    normalize(ctx.cwd().join(path))
}

/// Normalize expanded `self` fully, against `ctx.cwd()`.
///
/// This is equivalent to:
/// ```rust,ignore (illustrative)
/// let resolved = expand_tilde(path, ctx);
/// normalize(ctx.cwd().join(resolved))
/// ```
pub fn resolve_with_tilde(path: impl AsRef<Path>, ctx: &DuckContext) -> PathBuf {
    let resolved = expand_tilde(path, ctx);
    normalize(ctx.cwd().join(resolved))
}

/// Expand `~` into the `ctx.user_home()`.
pub fn expand_tilde(path: impl AsRef<Path>, ctx: &DuckContext) -> PathBuf {
    expand_tilde_with(path, ctx.user_home())
}

/// Expand `~` into a `home`.
pub fn expand_tilde_with(path: impl AsRef<Path>, home: impl AsRef<Path>) -> PathBuf {
    let path = path.as_ref();
    match path.strip_prefix("~") {
        Ok(rest) => home.as_ref().join(rest),
        Err(_) => {
            let os_str = path.as_os_str();
            let tilde = OsStr::new("~");
            if os_str
                .as_encoded_bytes()
                .starts_with(tilde.as_encoded_bytes())
            {
                warn!(path = ?path, "path (likely) contains a shell syntax for a different user's home directory (like `~user`), which is not supported");
            }
            path.to_path_buf()
        }
    }
}

/// Resolves both `self` and `other` and returns the relative path from `self` to `other`.
pub fn resolve_both_and_get_relative(
    path: impl AsRef<Path>,
    other: impl AsRef<Path>,
    ctx: &DuckContext,
) -> PathBuf {
    let source = resolve_with_tilde(path, ctx);
    let target = resolve_with_tilde(other, ctx);
    let mut src_components = source.components().fuse();
    let mut tgt_components = target.components().fuse();

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

/// Returns `true` if path exists on a disk and points to an executable file.
///
/// Current implementation only considers `unix` and `windows` cfg's, any other always returns
/// `false`.
pub fn is_executable(path: impl AsRef<Path>) -> bool {
    is_executable_inner(path.as_ref())
}

#[cfg(unix)]
fn is_executable_inner(path: &Path) -> bool {
    use std::os::unix::prelude::*;
    fs::metadata(path)
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
fn is_executable_inner(path: &Path) -> bool {
    path.is_file()
}

#[cfg(not(any(unix, windows)))]
fn is_executable_inner(_path: &Path) -> bool {
    false
}

/// A wrapper around [`std::fs::hard_link`].
pub fn hard_link(from: impl AsRef<Path>, to: impl AsRef<Path>) -> QuackResult<()> {
    let from = from.as_ref();
    let to = to.as_ref();
    fs::hard_link(from, to).with_context(|| {
        format!(
            "failed to hard link `{}` to `{}",
            from.display(),
            to.display()
        )
    })
}

/// A wrapper around [`std::fs::read`].
pub fn read_path(path: impl AsRef<Path>) -> QuackResult<Vec<u8>> {
    let path = path.as_ref();
    fs::read(path).with_context(|| format!("failed to read `{}`", path.display()))
}

/// A wrapper around [`std::fs::read_to_string`].
pub fn read_path_to_string(path: impl AsRef<Path>) -> QuackResult<String> {
    let path = path.as_ref();
    fs::read_to_string(path).with_context(|| format!("failed to read `{}`", path.display()))
}

/// A wrapper around [`std::fs::rename`].
pub fn rename(from: impl AsRef<Path>, to: impl AsRef<Path>) -> QuackResult<()> {
    let from = from.as_ref();
    let to = to.as_ref();
    fs::rename(from, to).with_context(|| {
        format!(
            "failed to rename `{}` to `{}`",
            from.display(),
            to.display()
        )
    })
}

/// A wrapper around [`std::fs::remove_file`].
pub fn remove_file(path: impl AsRef<Path>) -> QuackResult<()> {
    let path = path.as_ref();
    ignore_not_found(fs::remove_file(path))
        .with_context(|| format!("failed to remove file `{}`", path.display()))
}

/// A wrapper around [`std::fs::remove_dir`].
pub fn remove_dir(path: impl AsRef<Path>) -> QuackResult<()> {
    let path = path.as_ref();
    ignore_not_found(fs::remove_dir(path))
        .with_context(|| format!("failed to remove directory `{}`", path.display()))
}

/// A wrapper around [`std::fs::remove_dir_all`].
pub fn remove_tree(path: impl AsRef<Path>) -> QuackResult<()> {
    let path = path.as_ref();
    ignore_not_found(fs::remove_dir_all(path))
        .with_context(|| format!("failed to remove tree at `{}`", path.display()))
}

/// A wrapper around [`std::fs::set_permissions`].
pub fn set_permissions(path: impl AsRef<Path>, permissions: Permissions) -> QuackResult<()> {
    let path = path.as_ref();
    fs::set_permissions(path, permissions)
        .with_context(|| format!("failed to set permissions on `{}`", path.display()))
}

/// A wrapper around [`std::fs::write`].
pub fn write_file(path: impl AsRef<Path>, contents: impl AsRef<[u8]>) -> QuackResult<()> {
    let path = path.as_ref();
    fs::write(path, contents).with_context(|| format!("failed to write to `{}`", path.display()))
}

/// A wrapper around [`std::fs::metadata`].
pub fn metadata(path: impl AsRef<Path>) -> QuackResult<fs::Metadata> {
    let path = path.as_ref();
    fs::metadata(path).with_context(|| format!("failed to get metadata of `{}`", path.display()))
}

/// Get the last modification time.
pub fn mtime(path: impl AsRef<Path>) -> QuackResult<SystemTime> {
    let path = path.as_ref();
    mtime_inner(path)
        .with_context(|| format!("failed to get modification time of `{}`", path.display()))
}
fn mtime_inner(path: &Path) -> io::Result<SystemTime> {
    let metadata = fs::metadata(path)?;
    metadata.modified()
}

/// Get the last change time.
/// Important
/// ---------
/// This is not "creation time".
pub fn ctime(path: impl AsRef<Path>) -> QuackResult<SystemTime> {
    let path = path.as_ref();
    ctime_inner(path).with_context(|| format!("failed to get change time of `{}`", path.display()))
}

#[cfg(unix)]
fn ctime_inner(path: &Path) -> io::Result<SystemTime> {
    use std::os::unix::fs::MetadataExt;
    use std::time::Duration;

    let metadata = fs::metadata(path)?;
    let seconds = metadata.ctime() as u64;
    let nanos = metadata.ctime_nsec() as u64;
    Ok(SystemTime::UNIX_EPOCH + Duration::from_secs(seconds) + Duration::from_nanos(nanos))
}

#[cfg(windows)]
fn ctime_inner(path: &Path) -> io::Result<SystemTime> {
    // Returns `ChangeTime` from `$STANDARD_INFORMATION` attribute.
    // Documentation:
    // https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_basic_info
    use std::os::windows::io::AsRawHandle;
    use std::time::Duration;

    use windows_sys::Win32::Storage::FileSystem::{
        FILE_BASIC_INFO, FileBasicInfo, GetFileInformationByHandleEx,
    };

    let file = File::open(path)?;
    let handle = file.as_raw_handle();
    let mut info = FILE_BASIC_INFO::default();

    // SAFETY:
    // - `handle` is a valid file handle borrowed from `file`, which isn't closed before the call,
    // - `info` is a valid `FILE_BASIC_INFO`, therefore mutable pointers to it are valid for reads and writes,
    //   and are properly aligned,
    // - size is exactly `size_of::<FILE_BASIC_INFO>()` bytes,
    // - `FileBasicInfo` is the valid class for `FILE_BASIC_INFO` struct,
    // - OS ensures that `info` either is valid if the call succeeds, or isn't changed if the call fails.
    //   Either way, it will be a valid `FILE_BASIC_INFO`.
    let return_value = unsafe {
        GetFileInformationByHandleEx(
            handle,
            FileBasicInfo,
            &mut info as *mut _ as *mut _,
            std::mem::size_of::<FILE_BASIC_INFO>() as u32,
        )
    };
    if return_value == 0 {
        return Err(std::io::Error::last_os_error());
    }

    Ok(SystemTime::UNIX_EPOCH + Duration::from_nanos(info.ChangeTime as u64))
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
        let path = "/../../../cfg";
        assert_eq!(normalize(path), Path::new("/cfg"));

        let path = "/../../..";
        assert_eq!(normalize(path), Path::new("/"));

        let path = "/home/../.";
        assert_eq!(normalize(path), Path::new("/"));

        let path = "/home/duckling/./xd";
        assert_eq!(normalize(path), Path::new("/home/duckling/xd"));

        let path = "/home/duckling/./xd/..";
        assert_eq!(normalize(path), Path::new("/home/duckling"));

        let path = "../a/b/c/../../../../d";
        assert_eq!(normalize(path), Path::new("../../d"));
    }

    #[test]
    fn home_tests() {
        let home = Path::new("/home/duckling");
        let path = Path::new("~");
        assert_eq!(expand_tilde_with(path, home), home);

        let path = Path::new("~/");
        assert_eq!(expand_tilde_with(path, home), home);

        let path = Path::new("~/foo");
        assert_eq!(expand_tilde_with(path, home), home.join("foo"));

        let path = Path::new("~/foo/../bar");
        assert_eq!(
            expand_tilde_with(path, home),
            home.join("foo").join("..").join("bar")
        );

        let path = Path::new(".");
        assert_eq!(expand_tilde_with(path, home), path,);

        let path = Path::new("/duckling");
        assert_eq!(expand_tilde_with(path, home), path,);

        let path = Path::new("duckling/ducktype");
        assert_eq!(expand_tilde_with(path, home), path,);

        // Other users' syntax is not supported.
        let path = Path::new("~duck/");
        assert_eq!(expand_tilde_with(path, home), path,);

        let path = Path::new("~duck");
        assert_eq!(expand_tilde_with(path, home), path,);
    }

    #[test]
    fn relative_tests() {
        let ctx = DuckContext::default();

        let source = Path::new("foo/bar");
        let target = Path::new("x/y/z");
        let expected = Path::new("../../x/y/z");
        assert_eq!(
            resolve_both_and_get_relative(source, target, &ctx),
            expected
        );

        let source = Path::new("");
        let target = Path::new("x/y/z");
        let expected = Path::new("x/y/z");
        assert_eq!(
            resolve_both_and_get_relative(source, target, &ctx),
            expected
        );

        let source = Path::new("foo/x");
        let target = Path::new("foo/y");
        let expected = Path::new("../y");
        assert_eq!(
            resolve_both_and_get_relative(source, target, &ctx),
            expected
        );

        let source = Path::new("foo/bar");
        let target = Path::new("../x/../y/z");
        let expected = Path::new("../../../y/z");
        assert_eq!(
            resolve_both_and_get_relative(source, target, &ctx),
            expected
        );
    }
}
