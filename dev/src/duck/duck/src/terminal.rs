use std::fmt::Display;

use console::{Term, style};

#[derive(Debug)]
pub struct Terminal {
    term: Term,
    verbosity: Verbosity,
}

#[derive(Debug, Default)]
pub enum Verbosity {
    Quiet,
    #[default]
    Default,
    Verbose,
}

impl Verbosity {
    pub fn is_quiet(&self) -> bool {
        matches!(self, Verbosity::Quiet)
    }

    pub fn is_verbose(&self) -> bool {
        matches!(self, Verbosity::Verbose)
    }
}

impl Terminal {
    pub fn set_verbosity(&mut self, verbosity: Verbosity) {
        self.verbosity = verbosity;
    }

    pub fn stdout() -> Terminal {
        Terminal {
            term: Term::stdout(),
            verbosity: Verbosity::Default,
        }
    }

    pub fn stderr() -> Terminal {
        Terminal {
            term: Term::stderr(),
            verbosity: Verbosity::Default,
        }
    }

    fn print_impl(&self, text: impl FnOnce() -> String, verbose_only: bool) {
        if self.verbosity.is_quiet() {
            return;
        }
        if verbose_only && !self.verbosity.is_verbose() {
            return;
        };
        drop(self.term.write_line(&text()));
    }

    pub fn print(&self, text: impl Display) {
        self.print_impl(|| format!("{}", text), false);
    }

    pub fn error(&self, text: impl Display) {
        let full_text = || format!("{} {}", style("Error:").red().bold(), text);
        self.print_impl(full_text, false);
    }

    pub fn warning(&self, text: impl Display) {
        let full_text = || format!("{} {}", style("Warning:").yellow().bold(), text);
        self.print_impl(full_text, false);
    }

    pub fn critical(&self, text: impl Display) {
        let full_text = || format!("{} {}", style("Critical:").red().reverse().bold(), text);
        self.print_impl(full_text, false);
    }

    pub fn info(&self, text: impl Display) {
        let full_text = || format!("{} {}", style("Info:").cyan(), text);
        self.print_impl(full_text, false);
    }

    pub fn print_verbose(&self, text: impl Display) {
        self.print_impl(|| format!("{}", text), true);
    }

    pub fn error_verbose(&self, text: impl Display) {
        let full_text = || format!("{} {}", style("Error:").red().bold(), text);
        self.print_impl(full_text, true);
    }

    pub fn warning_verbose(&self, text: impl Display) {
        let full_text = || format!("{} {}", style("Warning:").yellow().bold(), text);
        self.print_impl(full_text, true);
    }

    pub fn critical_verbose(&self, text: impl Display) {
        let full_text = || format!("{} {}", style("Critical:").red().reverse().bold(), text);
        self.print_impl(full_text, true);
    }

    pub fn info_verbose(&self, text: impl Display) {
        let full_text = || format!("{} {}", style("Info:").cyan(), text);
        self.print_impl(full_text, true);
    }
}
