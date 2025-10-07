use console::{Term, style};

pub struct Terminal {
    term: Term,
    verbose: bool,
}

impl Terminal {
    pub fn set_verbose(&mut self) {
        self.verbose = true;
    }

    pub fn stdout() -> Terminal {
        Terminal {
            term: Term::stdout(),
            verbose: false,
        }
    }

    pub fn stderr() -> Terminal {
        Terminal {
            term: Term::stderr(),
            verbose: false,
        }
    }

    pub fn print(&self, text: &str, verbose_only: bool) {
        if verbose_only && !self.verbose {
            return;
        }

        let _ = self.term.write_line(text);
    }

    pub fn error(&self, text: &str, verbose_only: bool) {
        let full_text = format!("{} {}", style("Error:").red().bold(), text);
        self.print(&full_text, verbose_only);
    }

    pub fn warning(&self, text: &str, verbose_only: bool) {
        let full_text = format!("{} {}", style("Warning:").yellow().bold(), text);
        self.print(&full_text, verbose_only);
    }

    pub fn critical(&self, text: &str, verbose_only: bool) {
        let full_text = format!("{} {}", style("Critical:").red().reverse().bold(), text);
        self.print(&full_text, verbose_only);
    }

    pub fn info(&self, text: &str, verbose_only: bool) {
        let full_text = format!("{} {}", style("Info:").cyan(), text);
        self.print(&full_text, verbose_only);
    }
}
