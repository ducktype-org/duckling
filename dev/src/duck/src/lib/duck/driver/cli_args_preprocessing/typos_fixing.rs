// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Try to (wisely) fix user typos.
use std::ffi::OsString;

use clap::ArgMatches;
use itertools::Itertools;
use tracing::debug;

use crate::duck::driver::cli;
use crate::duck::driver::cli_args_preprocessing::builtin::{
    get_builtin_alias_expansion, get_builtin_aliases, is_builtin_subcommand,
};
use crate::duck::driver::cli_args_preprocessing::levenshtein;
use crate::duck::driver::external_subcommands::ExternalSubcommands;
use crate::duck::driver::subcommands::run_script::is_name_possible_script_path_subcmd;
use crate::duck::driver::subcommands::subcommands;
use crate::{DuckContext, QuackResult, qp_bail};

/// Try to fix user typos.
///
/// We're using [levenshtein distance](levenshtein::distance) for checking what is a typo.
pub fn fix_typos(
    args: ArgMatches,
    ctx: &DuckContext,
    external_cmds: &ExternalSubcommands,
) -> QuackResult<ArgMatches> {
    // No subcommand.
    let Some((name, subcmd_args)) = args.subcommand() else {
        return Ok(args);
    };

    if is_valid_subcmd(ctx, name, external_cmds)? {
        return Ok(args);
    }

    let targets = possible_targets(ctx, external_cmds)?;
    debug!(
        %name,
        ?targets,
        "Testing levenshtein",
    );
    let closest_targets = find_closest_targets(name, &targets, ctx.duck_cfg().max_fix_dist()?);

    let Some((&first, rest)) = closest_targets.as_slice().split_first() else {
        return Ok(args);
    };

    if ctx.duck_cfg().fixes_enabled()? && rest.is_empty() {
        let new_args = fix(name, first, subcmd_args)?;
        Ok(new_args)
    } else {
        qp_bail!("{}", make_levenshtein_nofix_msg(name, &closest_targets))
    }
}

/// Check, if command `name` is a valid duck subcommand.
///
/// Valid subcommands are:
/// - builtin subcommands,
/// - builtin aliases,
/// - user-defined aliases,
/// - external subcommands,
/// - anything that resembles a path to a script.
fn is_valid_subcmd(
    ctx: &DuckContext,
    name: &str,
    external_cmds: &ExternalSubcommands,
) -> QuackResult<bool> {
    Ok(is_builtin_subcommand(name)
        || get_builtin_alias_expansion(name).is_some()
        || ctx.duck_cfg().alias_for(name)?.is_some()
        || is_name_possible_script_path_subcmd(name)
        || external_cmds.load(ctx).contains_key(name))
}

/// Get all known and valid subcommands.
///
/// This is a list containing all values for which [`is_valid_subcmd`] returns true.
fn possible_targets(
    ctx: &DuckContext,
    external_cmds: &ExternalSubcommands,
) -> QuackResult<Vec<String>> {
    let mut targets = subcommands()
        .into_iter()
        .map(|x| x.get_name().to_string())
        .collect::<Vec<_>>();
    let aliases = ctx.duck_cfg().aliases()?;
    targets.extend(aliases.0.into_keys());
    targets.extend(external_cmds.load(ctx).keys().cloned());
    targets.extend(get_builtin_aliases().map(str::to_string));
    Ok(targets)
}

/// Get all closest targets to the `bad_cmd`, which are no further than `max_fix_dist` (in terms of
/// the Levenshtein distance).
fn find_closest_targets<'a>(
    bad_cmd: &str,
    targets: &'a [String],
    max_fix_dist: u64,
) -> Vec<&'a str> {
    targets
        .iter()
        .fold(Vec::new(), |acc, target| {
            update_closest_targets(acc, target, bad_cmd, max_fix_dist)
        })
        .into_iter()
        .map(|(target, _)| target)
        .collect()
}

/// Update closest targets given the actual stack, target, and `max_fix_dist`.
///
/// This function will __only__ keep those values, which are (currently) closest to the target,
/// and all have exactly the same distance.
fn update_closest_targets<'a>(
    mut acc: Vec<(&'a str, u64)>,
    target: &'a str,
    bad_cmd: &str,
    max_fix_dist: u64,
) -> Vec<(&'a str, u64)> {
    let dist = levenshtein::distance(bad_cmd, target) as u64;
    if dist > max_fix_dist {
        return acc;
    }
    match acc.len() {
        0 => {
            acc.push((target, dist));
        }
        _ => match (dist < acc[0].1, dist == acc[0].1) {
            (true, false) => {
                acc.clear();
                acc.push((target, dist));
            }
            (false, true) => {
                acc.push((target, dist));
            }
            _ => {}
        },
    }
    acc
}

/// Helper for creating a common message if we didn't fix a typo.
fn make_levenshtein_nofix_msg(bad_cmd: &str, closest_targets: &[&str]) -> String {
    let suggestions = closest_targets
        .iter()
        .map(|target| format!("- `{target}`"))
        .join("\n");
    format!("no such command as `{bad_cmd}`; did you mean:\n{suggestions}?")
}

/// We'd guessed that `bad_cmd` can be replaced by `new_subcmd`.
///
/// Replace it and re-parse the arguments.
fn fix(bad_cmd: &str, new_subcmd: &str, subcmd_args: &ArgMatches) -> QuackResult<ArgMatches> {
    debug!(%bad_cmd, %new_subcmd, "changed subcommand");
    let new_cli_args = make_cli_args(new_subcmd, subcmd_args);
    parse_fixed_args(new_cli_args)
}

/// Create new CLI arguments for a fixed subcommand.
fn make_cli_args(fixed_cmd: &str, subcmd_args: &ArgMatches) -> Vec<OsString> {
    let mut result = vec![OsString::from(fixed_cmd)];
    result.extend(
        subcmd_args
            .get_many::<OsString>("")
            .unwrap_or_default()
            .cloned(),
    );
    result
}

/// Re-parse fixed arguments.
fn parse_fixed_args(new_cli_args: Vec<OsString>) -> QuackResult<ArgMatches> {
    Ok(cli()
        .no_binary_name(true)
        .try_get_matches_from(new_cli_args)?)
}

#[cfg(test)]
mod tests {
    use std::collections::HashMap;

    use tempfile::{TempDir, tempdir};

    use super::*;
    use crate::util::path_ops_ext::PathOpsExt;
    use crate::util::test_utils::setup_test;

    fn setup_duck_home_with_a_given_max_fix_distance(dist: u64) -> (DuckContext, TempDir) {
        // We set cache directory to a temporary directory, so we can use `Fetcher` without
        // worrying about leaving traces of tests in FS.
        let dir = tempdir().unwrap();
        // SAFETY: Setup is single threaded, and `Env` in `DuckContext`, copies all envs.
        unsafe {
            std::env::set_var("DUCK_HOME", dir.path());
        }
        dir.path()
            .join("config.yaml")
            .write(format!(
                "security:
  typos:
    enabled: true
    max-distance: {dist}",
            ))
            .unwrap();
        let ctx = DuckContext::default();
        assert_eq!(ctx.duck_cfg().max_fix_dist().unwrap(), dist);
        assert!(ctx.duck_cfg().fixes_enabled().unwrap());
        // SAFETY: Setup is single threaded, and `Env` in `DuckContext`, copies all envs.
        unsafe {
            std::env::remove_var("DUCK_HOME");
        }
        (ctx, dir)
    }

    #[test]
    fn test_fixes() {
        let args_matches = cli().try_get_matches_from(["duck", "buil"]).unwrap();
        let (ctx, _dir) = setup_test(|| setup_duck_home_with_a_given_max_fix_distance(1));
        let external_cmds = ExternalSubcommands::default();
        let result = fix_typos(args_matches, &ctx, &external_cmds).unwrap();
        assert_eq!(result.subcommand_name(), Some("build"));
    }

    #[test]
    fn test_multiple_targets() {
        let args_matches = cli().try_get_matches_from(["duck", "inaa"]).unwrap();
        let (ctx, _dir) = setup_test(|| setup_duck_home_with_a_given_max_fix_distance(100));
        let external_cmds = ExternalSubcommands::default();
        let result = fix_typos(args_matches, &ctx, &external_cmds).expect_err(
            "There are two equally distant targets (`info` and `init`), so fixing should fail.",
        );
        assert_eq!(
            result.to_string(),
            "no such command as `inaa`; did you mean:\n\
            - `init`\n\
            - `info`?"
        );
    }

    #[test]
    fn test_single_closest_target() {
        let args_matches = cli().try_get_matches_from(["duck", "ini", "f"]).unwrap();
        let (ctx, _dir) = setup_test(|| setup_duck_home_with_a_given_max_fix_distance(100));
        let external_cmds = ExternalSubcommands::default();
        let result = fix_typos(args_matches, &ctx, &external_cmds).unwrap();
        assert_eq!(result.subcommand_name(), Some("init"));
    }

    #[test]
    fn test_fixes_to_alias() {
        let fake_aliases = HashMap::from([(
            String::from("my_alias"),
            String::from("expands to something -a --b c"),
        )]);

        let args_matches = cli().try_get_matches_from(["duck", "ny_aias"]).unwrap();
        let (mut ctx, _dir) = setup_test(|| setup_duck_home_with_a_given_max_fix_distance(2));
        ctx.duck_cfg_mut().set_aliases(fake_aliases);
        let external_cmds = ExternalSubcommands::default();
        let result = fix_typos(args_matches, &ctx, &external_cmds).unwrap();
        assert_eq!(result.subcommand_name(), Some("my_alias"));
    }

    #[test]
    fn test_tries_fixing_to_builtin_alias() {
        let args_matches = cli().try_get_matches_from(["duck", "a"]).unwrap();
        let (ctx, _dir) = setup_test(|| setup_duck_home_with_a_given_max_fix_distance(1));
        let result = fix_typos(args_matches, &ctx, &ExternalSubcommands::default()).expect_err(
            "There are two equally distant targets (`b` and `r`), so fixing should fail.",
        );
        assert_eq!(
            result.to_string(),
            "no such command as `a`; did you mean:\n- `b`\n- `r`?"
        );
    }
}
