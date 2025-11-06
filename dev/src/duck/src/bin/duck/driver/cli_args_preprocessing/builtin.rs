use tracing::debug;

use crate::driver::subcommands::exec_for;

// All builtin aliases should be set here.
// Format is `(alias, command)`. Current code assumes only „simple” aliases,
// f.e. `("t", "test")` is fine, but not `("foo", "build --help")`.
// It's guarded by `driver::no_aliases_in_parser()` test.
const BUILTIN_ALIASES: [(&str, &str); 2] = [("b", "build"), ("r", "run")];

pub fn get_builtin_alias_expansion(name: &str) -> Option<&'static str> {
    debug!("getting builtin alias for `{name}`");
    BUILTIN_ALIASES.iter().find_map(|(alias, expansion)| {
        if *alias == name {
            Some(*expansion)
        } else {
            None
        }
    })
}

pub fn get_builtin_aliases() -> impl Iterator<Item = &'static str> {
    BUILTIN_ALIASES.iter().map(|(k, _)| *k)
}

pub fn is_builtin_subcommand(name: &str) -> bool {
    exec_for(name).is_some()
}
