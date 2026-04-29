use tracing::debug;

use crate::duck::driver::subcommands::exec_for;

// All builtin aliases should be set here.
// Format is `(alias, command)`. Current code assumes only „simple” aliases,
// f.e. `("t", "test")` is fine, but not `("foo", "build --help")`.
// It's guarded by `driver::no_aliases_in_parser()` test.
const BUILTIN_ALIASES: [(&str, &str); 3] = [("b", "build"), ("r", "run"), ("rs", "run-script")];

/// Get expanded command for the alias `name`.
pub fn get_builtin_alias_expansion(name: &str) -> Option<&'static str> {
    debug!("getting the builtin alias for `{name}`");
    BUILTIN_ALIASES.iter().find_map(|(alias, expansion)| {
        if *alias == name {
            Some(*expansion)
        } else {
            None
        }
    })
}

/// Get all builtin aliases (keys).
pub fn get_builtin_aliases() -> impl Iterator<Item = &'static str> {
    BUILTIN_ALIASES.iter().map(|(k, _)| *k)
}

/// Check if `name` is 1-1 builtin subcommand.
///
/// Builtin aliases may (and should) return false.
///
/// To check if `name` is a builtin alias, use [`get_builtin_alias_expansion`] instead.
pub fn is_builtin_subcommand(name: &str) -> bool {
    exec_for(name).is_some()
}
