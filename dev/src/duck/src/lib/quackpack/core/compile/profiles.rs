use std::{collections::HashMap, sync::LazyLock};

use crate::StrId;

pub static PREDEFINED_PROFILES: LazyLock<HashMap<StrId, Profile>> = LazyLock::new(|| {
    [
        (
            "dev".into(),
            Profile {
                opt_level: OptLevel::One,
                dvm_bytecode: false,
                incremental: true,
                c_std: true,
            },
        ),
        (
            "release".into(),
            Profile {
                opt_level: OptLevel::Three,
                dvm_bytecode: false,
                incremental: false,
                c_std: true,
            },
        ),
        (
            "test".into(),
            Profile {
                opt_level: OptLevel::One,
                dvm_bytecode: false,
                incremental: true,
                c_std: true,
            },
        ),
        (
            "bench".into(),
            Profile {
                opt_level: OptLevel::Three,
                dvm_bytecode: false,
                incremental: false,
                c_std: true,
            },
        ),
    ]
    .into()
});

#[derive(Clone, Copy, Debug)]
/// List of specific options which should be passed to the compiler.
pub struct Profile {
    /// Set optimization level.
    /// Possible values are: 0, 1, 2, 3, s, z.
    /// See https://llvm.org/doxygen/classllvm_1_1OptimizationLevel.html
    pub opt_level: OptLevel,
    /// Compile to DVM bytecode instead of exe.
    pub dvm_bytecode: bool,
    /// When false, disable incremental compilation (do not load previous query graph).
    pub incremental: bool,
    /// When false, doesn't link the C standard library into the final executable.
    pub c_std: bool,
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
