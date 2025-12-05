use std::fmt::Display;
use std::io::Read;
use std::io::Write;

use console::{Term, WithoutAnsi, colors_enabled, colors_enabled_stderr, style};
use paste::item;

#[derive(Debug)]
pub struct Terminal {
    term: Term,
    verbosity: Verbosity,
    colors_enabled: bool,
}

#[derive(Debug, Default)]
pub enum Verbosity {
    Quiet,
    #[default]
    Default,
    Verbose,
}

macro_rules! delegate_styles {
    (
        $(
            $name:ident => $value:literal $(+ $opt:ident )* $(,)?
        ),*
    ) => {
    item! {
        $(
            pub fn $name(&self, text: impl ::std::fmt::Display) {
                let full_text = || format!("{} {}", style($value)$(.$opt())*, text);
                self.print_nl_impl(full_text, false);
            }

            pub fn [<$name _verbose>](&self, text: impl ::std::fmt::Display) {
                let full_text = || format!("{} {}", style($value)$(.$opt())*, text);
                self.print_nl_impl(full_text, true);
            }

            pub fn [<$name _no_nl>](&self, text: impl ::std::fmt::Display) {
                let full_text = || format!("{} {}", style($value)$(.$opt())*, text);
                self.print_impl(full_text, false);
            }
            pub fn [<$name _verbose_no_nl>](&self, text: impl ::std::fmt::Display) {
                let full_text = || format!("{} {}", style($value)$(.$opt())*, text);
                self.print_impl(full_text, true);
            }
        )*
    }
    };
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

    pub fn set_color(&mut self, colors_enabled: bool) {
        self.colors_enabled = colors_enabled;
    }

    pub fn stdout() -> Terminal {
        Terminal {
            term: Term::stdout(),
            verbosity: Verbosity::Default,
            colors_enabled: colors_enabled(),
        }
    }

    pub fn stderr() -> Terminal {
        Terminal {
            term: Term::stderr(),
            verbosity: Verbosity::Default,
            colors_enabled: colors_enabled_stderr(),
        }
    }

    fn print_nl_impl(&self, text: impl FnOnce() -> String, verbose_only: bool) {
        if self.verbosity.is_quiet() {
            return;
        }
        if verbose_only && !self.verbosity.is_verbose() {
            return;
        };
        if !self.colors_enabled {
            drop(
                self.term
                    .write_line(WithoutAnsi::new(&text()).to_string().as_str()),
            );
        } else {
            drop(self.term.write_line(&text()));
        }
    }

    fn print_impl(&self, text: impl FnOnce() -> String, verbose_only: bool) {
        if self.verbosity.is_quiet() {
            return;
        }
        if verbose_only && !self.verbosity.is_verbose() {
            return;
        };
        let mut term = &self.term;
        if !self.colors_enabled {
            drop(term.write_all(WithoutAnsi::new(&text()).to_string().as_bytes()));
        } else {
            drop(term.write_all(text().as_bytes()));
        }
    }

    pub fn print(&self, text: impl Display) {
        self.print_nl_impl(|| format!("{}", text), false);
    }

    pub fn print_verbose(&self, text: impl Display) {
        self.print_nl_impl(|| format!("{}", text), true);
    }

    pub fn print_no_nl(&self, text: impl Display) {
        self.print_impl(|| format!("{}", text), false);
    }

    pub fn print_verbose_no_nl(&self, text: impl Display) {
        self.print_impl(|| format!("{}", text), true);
    }

    delegate_styles! {
        error => "Error:" + red + bold,
        warning => "Warning:" + yellow + bold,
        info => "Info:" + cyan + bold,
        note => "Note:" + cyan + bold,
        hint => "Hint:" + cyan + bold,
        critical => "Critical:" + red + reverse + bold,
    }

    pub fn verbosity(&self) -> &Verbosity {
        &self.verbosity
    }

    pub fn term(&self) -> &Term {
        &self.term
    }
}

impl Write for Terminal {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        if self.verbosity.is_quiet() {
            Ok(0)
        } else {
            self.term.write(buf)
        }
    }

    fn flush(&mut self) -> std::io::Result<()> {
        self.term.flush()
    }
}

impl Write for &Terminal {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        let mut term: &Term = &self.term;
        if self.verbosity.is_quiet() {
            Ok(0)
        } else {
            term.write(buf)
        }
    }

    fn flush(&mut self) -> std::io::Result<()> {
        self.term.flush()
    }
}

impl Read for Terminal {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        self.term.read(buf)
    }
}

impl Read for &Terminal {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        let mut term: &Term = &self.term;
        term.read(buf)
    }
}
