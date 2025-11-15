use std::collections::HashMap;

use crate::StrId;

#[derive(Debug)]
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
pub struct CompilerFlagsMap(HashMap<StrId, CompilerSpecificOptions>);

impl CompilerFlagsMap {
    pub fn new(flags: HashMap<StrId, CompilerSpecificOptions>) -> Self {
        Self(flags)
    }

    pub fn options_for(&self, key: StrId) -> Option<&[StrId]> {
        self.0.get(&key).map(CompilerSpecificOptions::flags)
    }
}

#[derive(Debug)]
pub struct Profiles(CompilerFlagsMap);

impl Profiles {
    pub fn new(compiler_flags_map: CompilerFlagsMap) -> Self {
        Self(compiler_flags_map)
    }

    pub fn options_for(&self, key: StrId) -> Option<&[StrId]> {
        self.0.options_for(key)
    }
}

#[derive(Debug)]
pub struct Targets(CompilerFlagsMap);

impl Targets {
    pub fn new(compiler_flags_map: CompilerFlagsMap) -> Self {
        Self(compiler_flags_map)
    }

    pub fn options_for(&self, key: StrId) -> Option<&[StrId]> {
        self.0.options_for(key)
    }
}
