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
    fn test_parser() {
        cli().debug_assert();
    }
}
