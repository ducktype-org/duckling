use clap::{Command, crate_name, crate_version};

pub mod cli_args_preprocessing;
pub(crate) mod cli_ext;
pub mod global_options;
pub mod run;
pub mod styles;
pub mod subcommands;

use cli_ext::CommandExt;

use crate::duck::driver::{
    cli_ext::{flag, optional},
    styles::get_styles,
};

/// Create main cli parser.
fn cli() -> Command {
    let style = *styles::get_styles().get_literal();
    let after_help = format!(
        "To run a script you can also use syntax `{style}duck [OPTIONS] <path-to-script>{style:#}`"
    );
    Command::new(crate_name!())
        .version(crate_version!())
        .arg(
            flag("verbose", "Use more verbose output")
                .conflicts_with("quiet")
                .short('v')
                .global(true),
        )
        .arg(
            flag("quiet", "Suppress all output")
                .short('q')
                .conflicts_with("verbose")
                .global(true),
        )
        .add_chdir()
        .arg(
            optional("color", "Control the colored output")
                .value_parser(["always", "never", "auto"])
                .global(true),
        )
        .arg(flag("offline", "Don't perform any network requests").global(true))
        .allow_external_subcommands(true)
        .subcommands(subcommands::subcommands())
        .styles(get_styles())
        .after_help(after_help)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn validate_parser() {
        cli().debug_assert();
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
