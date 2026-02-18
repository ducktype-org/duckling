use std::{
    fs::OpenOptions,
    io::{self, Read, Write},
    path::Path,
};

use crate::QuackResult;

const BUFFER_SIZE: usize = 4096;

pub trait PathOpsExt {
    /// Copy the contents of one file into another and synchronize the result to disk.
    fn copy_file_to<P: AsRef<Path>>(&self, to: P) -> QuackResult<()>;

    /// Ensure that directory-level changes (creation, deletion, renaming) are persisted to disk.
    ///
    /// This is a best-effort operation. If unsupported, it will be silently skipped.
    fn try_fsync_dir(&self) -> QuackResult<()>;
}

impl PathOpsExt for Path {
    fn copy_file_to<P: AsRef<Path>>(&self, to: P) -> QuackResult<()> {
        let mut source = {
            let mut opts = OpenOptions::new();
            opts.read(true).open(self)
        }?;
        let mut target = {
            let mut opts = OpenOptions::new();
            opts.write(true).create(true).open(to)
        }?;
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

    fn try_fsync_dir(&self) -> QuackResult<()> {
        let dir = {
            let mut opts = OpenOptions::new();
            opts.read(true).open(self)
        }?;
        match dir.sync_data() {
            Ok(_) => Ok(()),
            Err(e) if matches!(e.kind(), io::ErrorKind::Unsupported) => Ok(()),
            Err(e) => Err(e.into()),
        }
    }
}
