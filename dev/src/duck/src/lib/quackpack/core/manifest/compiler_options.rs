use std::collections::HashMap;

use crate::StrId;

#[derive(Debug)]
/// List of specific options which should be passed to the compiler.
pub struct CompilerSpecificOptions {
    flags: Vec<StrId>,
}

impl CompilerSpecificOptions {
    pub fn new(flags: Vec<StrId>) -> Self {
        Self { flags }
    }

    pub fn flags(&self) -> &[StrId] {
        &self.flags
    }
}

#[derive(Debug)]
/// Common helper for `Profiles` and `Targets` structs.
struct CompilerFlagsMap(HashMap<StrId, CompilerSpecificOptions>);

impl CompilerFlagsMap {
    fn options_for(&self, key: StrId) -> Option<&[StrId]> {
        self.0.get(&key).map(CompilerSpecificOptions::flags)
    }
}

#[derive(Debug)]
/// Map `profile name <-> options for compiler`
pub struct Profiles(CompilerFlagsMap);

impl Profiles {
    pub fn new(compiler_flags: HashMap<StrId, CompilerSpecificOptions>) -> Self {
        Self(CompilerFlagsMap(compiler_flags))
    }

    pub fn options_for(&self, key: StrId) -> Option<&[StrId]> {
        self.0.options_for(key)
    }
}

#[derive(Debug)]
/// Map `target name <-> options for compiler`
pub struct Targets(CompilerFlagsMap);

impl Targets {
    pub fn new(compiler_flags: HashMap<StrId, CompilerSpecificOptions>) -> Self {
        Self(CompilerFlagsMap(compiler_flags))
    }

    pub fn options_for(&self, key: StrId) -> Option<&[StrId]> {
        self.0.options_for(key)
    }
}
