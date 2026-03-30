use clap::{Command, crate_name, crate_version};

pub mod cli_args_preprocessing;
pub(crate) mod cli_ext;
pub mod global_options;
pub mod run;
pub mod styles;
pub mod subcommands;

use cli_ext::CommandExt;

use crate::duck::driver::styles::get_styles;

/// Create main cli parser.
fn cli() -> Command {
    let style = *styles::get_styles().get_literal();
    let after_help = format!(
        "To run a script you can also use syntax `{style}duck [OPTIONS] <path-to-script>{style:#}`"
    );
    Command::new(crate_name!())
        .version(crate_version!())
        .add_verbose()
        .add_quiet()
        .add_chdir()
        .add_color()
        .add_offline()
        .allow_external_subcommands(true)
        .subcommands(subcommands::subcommands())
        .styles(get_styles())
        .after_help(after_help)
}

/// Same as [`cli`], but ignores any errors and `help`/`--help` early exits.
fn cli_no_err() -> Command {
    cli()
        .disable_help_subcommand(true)
        .disable_help_flag(true)
        .ignore_errors(true)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn validate_parser() {
        cli().debug_assert();
        cli_no_err().debug_assert();
    }

    #[test]
    fn no_aliases_in_parser() {
        let cli = cli();
        assert_no_aliases_recursive(&cli);
    }

    fn assert_no_aliases_recursive(parser: &Command) {
        if parser.get_all_aliases().next().is_some() {
            panic!(
                "parser for the `{}` subcommand has set aliases via clap \
                all aliases should be set in `driver/cli_args_preprocessing/builtin.rs`",
                parser.get_name()
            );
        }
        for subcmd in parser.get_subcommands() {
            assert_no_aliases_recursive(subcmd);
        }
    }
}
