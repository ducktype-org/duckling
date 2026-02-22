use std::collections::HashMap;

use crate::StrId;
use crate::quackpack::schemas::registry;

#[derive(Clone, Debug)]
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

impl From<registry::CompilerOptions> for CompilerSpecificOptions {
    fn from(value: registry::CompilerOptions) -> Self {
        let flags = value.compiler_flags.into_iter().map(Into::into).collect();
        Self { flags }
    }
}

impl From<CompilerSpecificOptions> for registry::CompilerOptions {
    fn from(value: CompilerSpecificOptions) -> Self {
        let compiler_flags = value.flags.into_iter().map(Into::into).collect();
        Self { compiler_flags }
    }
}

#[derive(Clone, Debug)]
/// Common helper for `Profiles` and `Targets` structs.
struct CompilerFlagsMap(HashMap<StrId, CompilerSpecificOptions>);

impl CompilerFlagsMap {
    fn options_for(&self, key: StrId) -> Option<&[StrId]> {
        self.0.get(&key).map(CompilerSpecificOptions::flags)
    }
}

impl From<HashMap<String, registry::CompilerOptions>> for CompilerFlagsMap {
    fn from(value: HashMap<String, registry::CompilerOptions>) -> Self {
        Self(
            value
                .into_iter()
                .map(|(k, v)| (k.into(), v.into()))
                .collect(),
        )
    }
}

impl From<CompilerFlagsMap> for HashMap<String, registry::CompilerOptions> {
    fn from(value: CompilerFlagsMap) -> Self {
        value
            .0
            .into_iter()
            .map(|(k, v)| (k.into(), v.into()))
            .collect()
    }
}

#[derive(Clone, Debug)]
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

impl From<HashMap<String, registry::CompilerOptions>> for Profiles {
    fn from(value: HashMap<String, registry::CompilerOptions>) -> Self {
        Self(value.into())
    }
}

impl From<Profiles> for HashMap<String, registry::CompilerOptions> {
    fn from(value: Profiles) -> Self {
        value.0.into()
    }
}
