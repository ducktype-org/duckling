use tracing::debug;

use crate::driver::subcommands::exec_for;

// TODO(Stas): We can either hardcode theme here, all collect them from cli() function.
//             But it's low priority for now.
const BUILTIN_ALIASES: [(&str, &str); 2] = [("b", "build"), ("r", "run")];

pub fn get_builtin_alias(name: &str) -> Option<&'static str> {
    debug!("getting builtin alias for `{name}`");
    BUILTIN_ALIASES.iter().find_map(|(alias, expansion)| {
        if *alias == name {
            Some(*expansion)
        } else {
            None
        }
    })
}

pub fn is_builtin_subcommand(name: &str) -> bool {
    exec_for(name).is_some()
}
