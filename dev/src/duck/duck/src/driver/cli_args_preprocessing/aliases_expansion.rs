use std::{collections::HashMap, ffi::OsString, path::PathBuf};

use crate::{
    DuckCtx,
    driver::{
        cli,
        cli_args_preprocessing::builtin::{get_builtin_alias, is_builtin_subcommand},
    },
};
use anyhow::bail;
use clap::ArgMatches;
use quackpack::QuackResult;
use tracing::debug;

pub fn expand_aliases(
    args: ArgMatches,
    ctx: &DuckCtx,
    external_cmds: &HashMap<String, PathBuf>,
    mut visited: Vec<String>,
) -> QuackResult<ArgMatches> {
    let Some((subcmd, subcmd_args)) = args.subcommand() else {
        return Ok(args);
    };
    match (
        is_builtin_subcommand(subcmd),
        ctx.alias_for(subcmd),
        external_cmds.contains_key(subcmd),
        get_builtin_alias(subcmd),
    ) {
        // TODO: This will be warned on, when inferring cmd.
        (_, Ok(None) | Err(_), _, _) => Ok(args),
        (true, Ok(Some(new)), false, None) => shadows_builtin_subcmd(ctx, &new, args),
        (false, Ok(Some(new)), true, None) => shadows_external_subcmd(ctx, &new, args),
        (false, Ok(Some(new)), false, Some(_)) => shadows_builtin_alias(ctx, &new, args),
        (false, Ok(Some(new)), false, None) => {
            // This is the actually interesting part.
            let new_args = expand_single_alias(subcmd, subcmd_args, &new, &mut visited)?;
            expand_aliases(new_args, ctx, external_cmds, visited)
        }
        _ => Ok(args),
    }
}

fn shadows_builtin_subcmd(ctx: &DuckCtx, alias: &str, args: ArgMatches) -> QuackResult<ArgMatches> {
    ctx.error_console().warning(format!(
        "user-defined alias `{alias}` shadows builtin subcommand, ignoring it..."
    ));
    Ok(args)
}

fn shadows_external_subcmd(
    ctx: &DuckCtx,
    alias: &str,
    args: ArgMatches,
) -> QuackResult<ArgMatches> {
    ctx.error_console().warning(format!(
        "user-defined alias `{alias}` shadows external subcommand, ignoring it..."
    ));
    Ok(args)
}

fn shadows_builtin_alias(ctx: &DuckCtx, alias: &str, args: ArgMatches) -> QuackResult<ArgMatches> {
    ctx.error_console().warning(format!(
        "user-defined alias `{alias}` shadows builtin alias, ignoring it..."
    ));
    Ok(args)
}

fn expand_single_alias(
    alias: &str,
    alias_args: &ArgMatches,
    alias_expansion: &str,
    visited: &mut Vec<String>,
) -> QuackResult<ArgMatches> {
    let new_cli_args = args_from_alias(alias_expansion, alias_args);
    debug!("replaced alias `{alias}` with `{new_cli_args:?}`");
    let parsed = parse_alias_args(new_cli_args)?;
    let Some(new_subcmd) = parsed.subcommand_name() else {
        bail!("user-defined alias `{alias}` does not have subcommand")
    };
    visited.push(alias.into());
    check_alias_cycle(alias, new_subcmd, visited)?;
    Ok(parsed)
}

fn args_from_alias(alias: &str, subcmd_args: &ArgMatches) -> Vec<OsString> {
    let mut result = alias.split(' ').map(OsString::from).collect::<Vec<_>>();
    result.extend(
        subcmd_args
            .get_many::<OsString>("")
            .unwrap_or_default()
            .cloned(),
    );
    result
}

fn parse_alias_args(new_cli_args: Vec<OsString>) -> QuackResult<ArgMatches> {
    Ok(cli()
        .no_binary_name(true)
        .try_get_matches_from(new_cli_args)?)
}

fn check_alias_cycle(current: &str, next: &str, visited: &[String]) -> QuackResult<()> {
    if visited.contains(&next.into()) {
        bail!(
            "user-defined alias `{current}` cycles: {} -> {next}",
            visited.join(" -> "),
        );
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    #![cfg(any(
        all(target_os = "linux", target_arch = "aarch64"),
        all(target_os = "linux", target_arch = "x86_64"),
        all(target_os = "windows", target_arch = "aarch64"),
        all(target_os = "windows", target_arch = "x86_64")
    ))]
    use super::*;
    use injectorpp::interface::injector::*;

    #[test]
    fn test_cycles() {
        fn fake_alias_for(name: &str) -> Option<String> {
            match name {
                "x" => Some(String::from("y --a")),
                "y" => Some(String::from("z --b xd")),
                "z" => Some(String::from("x")),
                _ => None,
            }
        }

        let mut injector = InjectorPP::new();
        injector
            .when_called(injectorpp::func!(fn (DuckCtx::alias_for)(&DuckCtx, &str) -> QuackResult<Option<String>>))
            .will_execute(injectorpp::fake!(
                func_type: fn(_x: &DuckCtx, name: &str) -> QuackResult<Option<String>>,
                returns: Ok(fake_alias_for(name))
            ));

        let args_matches = cli().try_get_matches_from(["duck", "x"]).unwrap();
        let ctx = DuckCtx::new().unwrap();
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
        fn fake_alias_for(name: &str) -> Option<String> {
            match name {
                "x" => Some(String::from("y")),
                "y" => Some(String::from("z --all-features")),
                "z" => Some(String::from("build")),
                _ => None,
            }
        }

        let mut injector = InjectorPP::new();
        injector
            .when_called(injectorpp::func!(fn (DuckCtx::alias_for)(&DuckCtx, &str) -> QuackResult<Option<String>>))
            .will_execute(injectorpp::fake!(
                func_type: fn(_x: &DuckCtx, name: &str) -> QuackResult<Option<String>>,
                returns: Ok(fake_alias_for(name))
            ));

        let args_matches = cli().try_get_matches_from(["duck", "x"]).unwrap();
        let ctx = DuckCtx::new().unwrap();
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
