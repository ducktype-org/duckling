//! Various drop guards.

use tracing::error;

use crate::util::file_locks::LockedFile;
use crate::util::path_ops_ext::PathOpsExt;
use crate::util::set_once::SetOnce;

/// Remove a file on drop, if armed.
/// That way we don't leave files with length 0 in FS in case of an error.
pub struct RemoveOnDrop {
    /// Always some.
    ///
    /// We need to close the file before removing it (otherwise it can fail on Windows).
    ///
    /// And the easiest way to remove it is through [`Drop`], which requires ownership.
    file: Option<LockedFile>,
    /// Whether we should remove the file.
    disarmed: SetOnce,
}

impl RemoveOnDrop {
    /// Create a new [`RemoveOnDrop`].
    ///
    /// Unless [`disarm`](Self::disarm) was called, [`drop`]ping this guard will call
    /// [`PathOpsExt::rm`].
    pub fn new(file: LockedFile) -> Self {
        Self {
            file: Some(file),
            disarmed: SetOnce::new(),
        }
    }

    /// Get a reference to the underlying [`LockedFile`].
    pub fn file(&self) -> &LockedFile {
        self.file.as_ref().expect("always Some")
    }

    /// Get a mutable reference to the underlying [`LockedFile`].
    pub fn file_mut(&mut self) -> &mut LockedFile {
        self.file.as_mut().expect("always Some")
    }

    /// Disarm this [`RemoveOnDrop`].
    ///
    /// “Disarm” means “don't remove file on drop”, however [`Drop`] of [`LockedFile`] is still called.
    pub fn disarm(&mut self) {
        self.disarmed.set();
    }
}

impl Drop for RemoveOnDrop {
    fn drop(&mut self) {
        if self.disarmed.was_set() {
            return;
        }
        let file = self.file.take().expect("always Some");
        let path = file.path().to_path_buf();
        // Close the file. Otherwise we can fail on Windows.
        drop(file);
        if let Err(e) = path.rm() {
            error!(error = %e, ?path, "failed to remove");
        }
    }
}
