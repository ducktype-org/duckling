use crate::{
    DuckCtx,
    driver::{cli, subcommands::exec_for},
};
use anyhow::bail;
use quackpack::QuackResult;
use tracing::debug;

pub(crate) fn run(_ctx: &mut DuckCtx) -> QuackResult<()> {
    let cli = cli();
    let matches = cli.try_get_matches()?;
    bail!("Implement main")
}

fn is_builtin_subcommand(name: &str) -> bool {
    exec_for(name).is_some()
}
