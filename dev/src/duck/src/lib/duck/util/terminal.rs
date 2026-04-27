use std::fmt::{self, Display};
use std::io::{Read, Write};
use std::str::FromStr;

use console::{Term, WithoutAnsi, colors_enabled, colors_enabled_stderr, style};
use dialoguer::Input;

use crate::QuackResult;
use crate::duck::util::indent::indent;

/// A struct which is responsible for printing to stdout/stderr.
pub struct Terminal {
    term: Term,
    verbosity: Verbosity,
    colors_enabled: bool,
}

impl fmt::Debug for Terminal {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Terminal")
            .field("verbosity", &self.verbosity)
            .field("colors_enabled", &self.colors_enabled)
            .finish_non_exhaustive()
    }
}

#[derive(Debug, Default)]
/// A verbosity of a [`Terminal`].
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
        #[inline]
        /// Print a
        #[doc = stringify!($name)]
        /// message to the terminal.
       pub fn $name(&self, text: impl ::std::fmt::Display) {
           let full_text = format!("{} {}", style($value)$(.$opt())*, text);
           let full_text = indent_without_first_line(full_text, $value.len() + 1);
           self.print(full_text)
       }

       #[inline]
        /// Print a verbose
        #[doc = stringify!($name)]
        /// message to the terminal.
       pub fn $verbose(&self, text: impl ::std::fmt::Display) {
           let full_text = format!("{} {}", style($value)$(.$opt())*, text);
           let full_text = indent_without_first_line(full_text, $value.len() + 1);
           self.print_verbose(full_text)
       }
    }
}

impl Verbosity {
    #[inline]
    /// Check, if this verbosity is quiet.
    pub fn is_quiet(&self) -> bool {
        matches!(self, Verbosity::Quiet)
    }

    #[inline]
    /// Check, if this verbosity is verbose.
    pub fn is_verbose(&self) -> bool {
        matches!(self, Verbosity::Verbose)
    }
}

impl Terminal {
    /// Set [`Verbosity`] on this [`Terminal`].
    pub fn set_verbosity(&mut self, verbosity: Verbosity) {
        self.verbosity = verbosity;
    }

    /// Set whether colors are enabled on this terminal.
    pub fn set_color(&mut self, colors_enabled: bool) {
        self.colors_enabled = colors_enabled;
    }

    /// Get the default [`Terminal`] for stdout.
    pub fn stdout() -> Terminal {
        Terminal {
            term: Term::stdout(),
            verbosity: Verbosity::Default,
            colors_enabled: colors_enabled(),
        }
    }

    /// Get the default [`Terminal`] for stderr.
    pub fn stderr() -> Terminal {
        Terminal {
            term: Term::stderr(),
            verbosity: Verbosity::Default,
            colors_enabled: colors_enabled_stderr(),
        }
    }

    /// Print a generic message to the terminal.
    ///
    /// If [`Verbosity`] is [`Quiet`](Verbosity::Quiet), this has no effect.
    pub fn print(&self, text: impl Display) {
        if self.verbosity.is_quiet() {
            return;
        }

        let text = text.to_string();
        if !self.colors_enabled {
            let _ = self.term.write_line(&WithoutAnsi::new(&text).to_string());
        } else {
            let _ = self.term.write_line(&text);
        }
    }

    #[inline]
    /// Print a generic verbose message to the terminal.
    ///
    /// If [`Verbosity`] is not [`Verbose`](Verbosity::Verbose), this has no effect.
    pub fn print_verbose(&self, text: impl Display) {
        if !self.verbosity.is_verbose() {
            return;
        }
        self.print(text)
    }

    delegate_styles! {
        FunctionName: error,
        VerboseName: error_verbose,
        Prefix: "error:",
        OptionalStyles: red + bold,
    }

    delegate_styles! {
        FunctionName: warning,
        VerboseName: warning_verbose,
        Prefix: "warning:",
        OptionalStyles: yellow + bold,
    }

    delegate_styles! {
        FunctionName: info,
        VerboseName: info_verbose,
        Prefix: "info:",
        OptionalStyles: cyan + bold,
    }

    delegate_styles! {
        FunctionName: note,
        VerboseName: note_verbose,
        Prefix: "note:",
        OptionalStyles: cyan + bold,
    }

    delegate_styles! {
        FunctionName: hint,
        VerboseName: hint_verbose,
        Prefix: "hint:",
        OptionalStyles: cyan + bold,
    }

    delegate_styles! {
        FunctionName: critical,
        VerboseName: critical_verbose,
        Prefix: "critical:",
        OptionalStyles: red + bold + reverse,
    }

    /// Get the [`Verbosity`] of this [`Terminal`].
    pub fn verbosity(&self) -> &Verbosity {
        &self.verbosity
    }

    /// Get the underlying [`Term`] used for printing.
    pub fn term(&self) -> &Term {
        &self.term
    }

    /// Get a [`String`] input from the user.
    pub fn prompt_once(&self, prompt: impl Into<String>) -> QuackResult<String> {
        Ok(Input::new()
            .with_prompt(prompt)
            .interact_text_on(&self.term)?)
    }

    /// Get a [`String`] input from the user, with a default value supplied.
    pub fn prompt_once_with_default(
        &self,
        prompt: impl Into<String>,
        default: String,
    ) -> QuackResult<String> {
        Ok(Input::new()
            .with_prompt(prompt)
            .default(default)
            .interact_text_on(&self.term)?)
    }

    /// Prompt user for an input until it can be correctly deserialized.
    pub fn prompt_until_valid<T>(&self, prompt: impl Into<String> + Clone) -> T
    where
        T: ToString + FromStr + Clone,
        <T as std::str::FromStr>::Err: std::fmt::Display,
    {
        loop {
            let input = Input::<'_, T>::new()
                .with_prompt(prompt.clone())
                .interact_text_on(&self.term);
            if let Ok(t) = input {
                return t;
            }
        }
    }

    /// Prompt user for an input until it can be correctly deserialized, with a default value supplied.
    pub fn prompt_until_valid_with_default<T>(
        &self,
        prompt: impl Into<String> + Clone,
        default: T,
    ) -> T
    where
        T: ToString + FromStr + Clone,
        <T as std::str::FromStr>::Err: std::fmt::Display,
    {
        loop {
            let input = Input::<'_, T>::new()
                .with_prompt(prompt.clone())
                .default(default.clone())
                .interact_text();
            if let Ok(t) = input {
                return t;
            }
        }
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

/// Indents all lines of `text` with `indentation`, expect for the first line.
fn indent_without_first_line(text: String, indentation: usize) -> String {
    let Some((first_line, rest)) = text.split_once('\n') else {
        return text;
    };
    let rest = indent(rest, indentation);
    format!("{first_line}\n{rest}")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn indent_tests() {
        assert_eq!(
            "",
            indent_without_first_line("".into(), 2),
            "indent ignores empty lines"
        );
        assert_eq!(
            "ala",
            indent_without_first_line("ala".into(), 2),
            "indent ignores first line"
        );
        assert_eq!(
            "\n",
            indent_without_first_line("\n".into(), 2),
            "indent should keep trailing newline"
        );
        assert_eq!(
            "ala
  ma
  kota",
            indent_without_first_line("ala\nma\nkota".into(), 2)
        );
        assert_eq!(
            "ala
  ma
  kota
",
            indent_without_first_line("ala\nma\nkota\n".into(), 2)
        );
    }
}
