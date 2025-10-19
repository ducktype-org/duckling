use clap::{Command, crate_name, crate_version};

pub mod cli_args_preprocessing;
pub(crate) mod cli_ext;
pub mod global_cli_options;
pub mod run;
pub mod subcommands;

use cli_ext::CommandExt;

fn cli() -> Command {
    Command::new(crate_name!())
        .version(crate_version!())
        .add_verbose()
        .add_quiet()
        .add_chdir()
        .add_color()
        .allow_external_subcommands(true)
        .subcommands(subcommands::subcommands())
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
                "parser for `{}` has set aliases via clap
                all aliases should be set in `driver/cli_args_preprocessing/builtin.rs`",
                parser.get_name()
            );
        }
        for subcmd in parser.get_subcommands() {
            assert_no_aliases_recursive(subcmd);
        }
    }
}
