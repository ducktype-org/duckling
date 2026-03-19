//! A cross-platform trait extension for replacing the current process with an another.

use std::convert::Infallible;
use std::process::Command;

use crate::QuackResult;
use crate::QuackResultContext;

/// Adds a portable [`exec_replace`](CommandExt::exec_replace) method to the [`Command`].
pub trait CommandExt {
    /// Replace the current process with command from [`Self`] and execute it.
    ///
    /// ## Returns
    ///
    /// [`Err`](crate::QuackError) variant means, that spawning new command failed.
    /// Otherwise this function shall never return.
    fn exec_replace(&mut self) -> QuackResult<Infallible>;
}

impl CommandExt for Command {
    #[cfg(unix)]
    fn exec_replace(&mut self) -> QuackResult<Infallible> {
        use std::os::unix::process::CommandExt;
        Err(self.exec()).with_context(|| {
            use itertools::Itertools;

            let name = self.get_program().display();
            let args = self.get_args().map(|arg| arg.display()).join(" ");
            let has_args = self.get_args().next().is_some();
            let command = if has_args {
                format!("{name} {args}")
            } else {
                name.to_string()
            };
            format!("failed to execute `{command}`")
        })
    }

    #[cfg(windows)]
    fn exec_replace(&mut self) -> QuackResult<Infallible> {
        use windows_sys::Win32::Foundation::{FALSE, TRUE};
        use windows_sys::Win32::System::Console::SetConsoleCtrlHandler;
        use windows_sys::core::BOOL;
        unsafe extern "system" fn handler(_: u32) -> BOOL {
            TRUE
        }
        unsafe {
            if SetConsoleCtrlHandler(Some(handler), TRUE) == FALSE {
                return Err(io::Error::other("failed to overwrite ctrl-c handler"));
            }
        }
        let status = self.spawn()?.wait()?;
        std::process::exit(status.code().unwrap_or(1))
    }

    #[cfg(not(any(unix, windows)))]
    fn exec_replace(&mut self) -> QuackResult<Infallible> {
        use crate::qp_bail_internal;

        qp_bail_internal!("implement `exec_replace`")
    }
}
