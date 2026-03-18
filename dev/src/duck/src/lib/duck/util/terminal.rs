use std::fmt::Display;
use std::io::Read;
use std::io::Write;

use console::{Term, WithoutAnsi, colors_enabled, colors_enabled_stderr, style};

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
        FunctionName: $name:ident,
        VerboseName: $verbose:ident,
        Prefix: $value:literal,
        OptionalStyles: $( $opt:ident $(+)? )* $(,)?
    ) => {
       pub fn $name(&self, text: impl ::std::fmt::Display) {
           let full_text = format!("{} {}", style($value)$(.$opt())*, text);
           self.print(full_text)
       }

       pub fn $verbose(&self, text: impl ::std::fmt::Display) {
           let full_text = format!("{} {}", style($value)$(.$opt())*, text);
           self.print_verbose(full_text)
       }
    }
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

    pub fn print(&self, text: impl Display) {
        if self.verbosity.is_quiet() {
            return;
        }
        self.print_verbose(text)
    }

    pub fn print_verbose(&self, text: impl Display) {
        let mut term = &self.term;
        let text = text.to_string();
        if !self.colors_enabled {
            drop(term.write_all(WithoutAnsi::new(&text).to_string().as_bytes()));
        } else {
            drop(term.write_all(text.as_bytes()));
        }
    }

    delegate_styles! {
        FunctionName: error,
        VerboseName: error_verbose,
        Prefix: "Error:",
        OptionalStyles: red + bold,
    }

    delegate_styles! {
        FunctionName: warning,
        VerboseName: warning_verbose,
        Prefix: "Warning:",
        OptionalStyles: yellow + bold,
    }

    delegate_styles! {
        FunctionName: info,
        VerboseName: info_verbose,
        Prefix: "Info:",
        OptionalStyles: cyan + bold,
    }

    delegate_styles! {
        FunctionName: note,
        VerboseName: note_verbose,
        Prefix: "Note:",
        OptionalStyles: cyan + bold,
    }

    delegate_styles! {
        FunctionName: hint,
        VerboseName: hint_verbose,
        Prefix: "Hint:",
        OptionalStyles: cyan + bold,
    }

    delegate_styles! {
        FunctionName: critical,
        VerboseName: critical_verbose,
        Prefix: "Critical:",
        OptionalStyles: red + bold + reverse,
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
