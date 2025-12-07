use std::{collections::HashMap, ffi::OsString, path::PathBuf};

use crate::{DuckCtx, QuackResult, qp_bail};
use clap::ArgMatches;
use itertools::chain;
use tracing::debug;

use crate::duck::driver::{
    cli,
    cli_args_preprocessing::builtin::{get_builtin_alias_expansion, is_builtin_subcommand},
};

pub fn expand_aliases(
    args: ArgMatches,
    ctx: &DuckCtx,
    external_cmds: &HashMap<String, PathBuf>,
    mut visited: Vec<String>,
) -> QuackResult<ArgMatches> {
    let Some((subcmd, subcmd_args)) = args.subcommand() else {
        return Ok(args);
    };
    let is_builtin = is_builtin_subcommand(subcmd);
    let alias = ctx.duck_cfg().alias_for(subcmd)?;
    let is_external = external_cmds.contains_key(subcmd);
    let builtin_alias = get_builtin_alias_expansion(subcmd);
    match (is_builtin, &alias, is_external, builtin_alias) {
        (false, None, true, Some(builtin)) => {
            ctx.error_console().warning(format!(
                "builtin alias `{subcmd}` shadows an external subcommand"
            ));
            expand_builtin_alias(builtin, subcmd_args)
        }
        (false, Some(_), false, Some(builtin)) => {
            ctx.error_console().warning(format!(
                "builtin alias `{subcmd}` shadows a user-defined alias"
            ));
            expand_builtin_alias(builtin, subcmd_args)
        }
        (false, Some(_), true, Some(builtin)) => {
            ctx.error_console().warning(format!(
                "builtin alias `{subcmd}` shadows a user-defined alias and an external subcommand"
            ));
            expand_builtin_alias(builtin, subcmd_args)
        }
        (false, None, false, Some(builtin)) => expand_builtin_alias(builtin, subcmd_args),
        (true, Some(_), false, None) => {
            ctx.error_console().warning(format!(
                "builtin subcommand `{subcmd}` shadows a user-defined alias"
            ));
            Ok(args)
        }
        (false, Some(_), true, None) => {
            ctx.error_console().warning(format!(
                "external subcommand `{subcmd}` shadows a user-defined alias"
            ));
            Ok(args)
        }
        (false, Some(new), false, None) => {
            // This is the actually interesting part.
            let new_args = expand_single_alias(subcmd, subcmd_args, new, &mut visited)?;
            expand_aliases(new_args, ctx, external_cmds, visited)
        }
        _ => {
            debug!(
                "expanding aliases: default branch with `{:?}`",
                (is_builtin, alias, is_external, builtin_alias)
            );
            Ok(args)
        }
    }
}

fn expand_builtin_alias(builtin: &str, args: &ArgMatches) -> QuackResult<ArgMatches> {
    let builtin = OsString::from(builtin);
    Ok(cli().no_binary_name(true).try_get_matches_from(chain(
        [&builtin],
        args.get_many::<OsString>("").unwrap_or_default(),
    ))?)
}

fn expand_single_alias(
    alias: &str,
    alias_args: &ArgMatches,
    alias_expansion: &str,
    visited: &mut Vec<String>,
) -> QuackResult<ArgMatches> {
    let new_cli_args = args_from_alias(alias_expansion, alias_args);
    debug!("replaced the alias `{alias}` with `{alias_expansion}`");
    let parsed = parse_alias_args(new_cli_args)?;
    let Some(new_subcmd) = parsed.subcommand_name() else {
        qp_bail!("user-defined alias `{alias}` does not have a subcommand")
    };
    visited.push(alias.into());
    check_alias_cycle(alias, new_subcmd, visited)?;
    Ok(parsed)
}

fn args_from_alias(alias: &str, subcmd_args: &ArgMatches) -> impl Iterator<Item = OsString> {
    let split = alias.split(' ').map(OsString::from);
    chain(
        split,
        subcmd_args
            .get_many::<OsString>("")
            .unwrap_or_default()
            .cloned(),
    )
}

fn parse_alias_args(new_cli_args: impl Iterator<Item = OsString>) -> QuackResult<ArgMatches> {
    Ok(cli()
        .no_binary_name(true)
        .try_get_matches_from(new_cli_args)?)
}

fn check_alias_cycle(current: &str, next: &str, visited: &[String]) -> QuackResult<()> {
    if visited.contains(&next.into()) {
        qp_bail!(
            "user-defined alias `{current}` cycles: {} -> {next}",
            visited.join(" -> "),
        );
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use std::collections::HashMap;

    use crate::{
        DuckCtx,
        duck::driver::{cli, cli_args_preprocessing::aliases_expansion::expand_aliases},
    };

    #[test]
    fn test_cycles() {
        let fake_aliases = HashMap::from([
            (String::from("x"), String::from("y --a")),
            (String::from("y"), String::from("z --b xd")),
            (String::from("z"), String::from("x")),
        ]);

        let args_matches = cli().try_get_matches_from(["duck", "x"]).unwrap();
        let mut ctx = DuckCtx::new().unwrap();
        ctx.duck_cfg_mut().set_aliases(fake_aliases);
        let external_cmds = HashMap::new();
        let visited = Vec::new();
        let result = expand_aliases(args_matches, &ctx, &external_cmds, visited);
        match result {
            Ok(_) => panic!(""),
            Err(err) => {
                assert_eq!(
                    err.to_string(),
                    "user-defined alias `z` cycles: x -> y -> z -> x"
                );
            }
        }
    }

    #[test]
    fn test_expands_ok() {
        let fake_aliases = HashMap::from([
            (String::from("x"), String::from("y")),
            (String::from("y"), String::from("z --all-features")),
            (String::from("z"), String::from("build")),
        ]);

        let args_matches = cli().try_get_matches_from(["duck", "x"]).unwrap();
        let mut ctx = DuckCtx::new().unwrap();
        ctx.duck_cfg_mut().set_aliases(fake_aliases);
        let external_cmds = HashMap::new();
        let visited = Vec::new();
        let result = expand_aliases(args_matches, &ctx, &external_cmds, visited);
        match result {
            Ok(new_args_matches) => {
                assert_eq!(new_args_matches.subcommand_name().unwrap(), "build");
            }
            Err(_) => panic!(""),
        }
    }
}
