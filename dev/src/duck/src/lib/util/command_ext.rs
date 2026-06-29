//! A cross-platform trait extension for replacing the current process with an another.

use std::convert::Infallible;
use std::fmt;
use std::process::Command;

use itertools::Itertools;

use crate::{QuackResult, QuackResultContext};

/// Adds a portable [`exec_replace`](CommandExt::exec_replace) method to the [`Command`].
pub trait CommandExt {
    /// Replace the current process with command from [`Self`] and execute it.
    ///
    /// ## Returns
    ///
    /// [`Err`](crate::QuackError) variant means, that spawning new command failed.
    /// Otherwise this function shall never return.
    fn exec_replace(&mut self) -> QuackResult<Infallible>;

    /// Return a wrapper struct for displaying this [`Command`].
    fn display(&self) -> impl fmt::Display;
}

impl CommandExt for Command {
    fn display(&self) -> impl fmt::Display {
        CommandDisplay { command: self }
    }

    #[cfg(unix)]
    fn exec_replace(&mut self) -> QuackResult<Infallible> {
        use std::os::unix::process::CommandExt;
        Err(self.exec()).with_context(|| format!("failed to execute `{}`", self.display()))
    }

    #[cfg(windows)]
    fn exec_replace(&mut self) -> QuackResult<Infallible> {
        use std::io;

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
        compile_error!("implement `exec_replace`")
    }
}

struct CommandDisplay<'a> {
    command: &'a Command,
}

impl fmt::Display for CommandDisplay<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut has_envs = false;
        let mut has_args = false;
        let envs = self
            .command
            .get_envs()
            .filter_map(|(k, v)| v.map(|v| (k, v)))
            .map(|(k, v)| {
                has_envs = true;
                format!("{}={}", k.display(), v.display())
            })
            .join(" ");
        let name = self.command.get_program().display();
        let args = self
            .command
            .get_args()
            .filter(|arg| !arg.is_empty())
            .map(|arg| {
                has_args = true;
                arg.display()
            })
            .join(" ");
        if has_envs {
            write!(f, "{envs} ")?;
        }
        write!(f, "{name}")?;
        if has_args {
            write!(f, " {args}")?;
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn display_basic() {
        let mut command = Command::new("x");
        command.arg("--help").arg("foo");
        assert_eq!(command.display().to_string(), "x --help foo");

        let command = Command::new("x");
        assert_eq!(command.display().to_string(), "x");

        let mut command = Command::new("x");
        command.env("KEY", "VALUE").env("KEY2", "VALUE2");
        assert_eq!(command.display().to_string(), "KEY=VALUE KEY2=VALUE2 x");

        let mut command = Command::new("x");
        command
            .env("KEY", "VALUE")
            .env("KEY2", "VALUE2")
            .arg("--help")
            .arg("foo");
        assert_eq!(
            command.display().to_string(),
            "KEY=VALUE KEY2=VALUE2 x --help foo"
        );
    }

    #[test]
    fn display_ignores_empty_args() {
        let mut command = Command::new("x");
        command.arg("");
        assert_eq!(command.display().to_string(), "x");
    }
}
