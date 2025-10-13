use crate::driver::subcommands::exec_for;

const BUILTIN_ALIASES: [(&str, &str); 2] = [("b", "build"), ("r", "run")];

pub fn get_builtin_alias(name: &str) -> Option<&'static str> {
    for (k, v) in BUILTIN_ALIASES {
        if k == name {
            return Some(v);
        }
    }
    None
}

pub fn is_builtin_subcommand(name: &str) -> bool {
    exec_for(name).is_some()
}
