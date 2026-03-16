use std::{collections::HashMap, sync::LazyLock};

use crate::StrId;

pub static PREDEFINED_PROFILES: LazyLock<HashMap<StrId, Profile>> = LazyLock::new(|| {
    [
        (
            "dev".into(),
            Profile {
                opt_level: OptLevel::Three,
                dvm_bytecode: false,
                no_incremental: false,
                no_c_std: false,
            },
        ),
        (
            "release".into(),
            Profile {
                opt_level: OptLevel::Three,
                dvm_bytecode: false,
                no_incremental: false,
                no_c_std: false,
            },
        ),
        (
            "test".into(),
            Profile {
                opt_level: OptLevel::Three,
                dvm_bytecode: false,
                no_incremental: false,
                no_c_std: false,
            },
        ),
        (
            "bench".into(),
            Profile {
                opt_level: OptLevel::Three,
                dvm_bytecode: false,
                no_incremental: false,
                no_c_std: false,
            },
        ),
    ]
    .into()
});

#[derive(Clone, Debug)]
/// List of specific options which should be passed to the compiler.
pub struct Profile {
    pub opt_level: OptLevel,
    pub dvm_bytecode: bool,
    pub no_incremental: bool,
    pub no_c_std: bool,
}

#[derive(Copy, Clone, Debug)]
/// Enum for different possible optimization levels in the compiler.
pub enum OptLevel {
    Zero,
    One,
    Two,
    Three,
    S,
    Z,
}
