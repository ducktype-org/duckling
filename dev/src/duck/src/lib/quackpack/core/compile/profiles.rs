use std::{collections::HashMap, fmt::Display, sync::LazyLock};

use crate::{QuackResult, QuackResultContext, StrId, qp_err, quackpack::core::manifest};

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

pub static DEFAULT_PROFILE: LazyLock<Profile> = LazyLock::new(Profile::default);

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

#[derive(Copy, Clone, Debug, Default)]
/// Enum for different possible optimization levels in the compiler.
pub enum OptLevel {
    Zero,
    One,
    Two,
    #[default]
    Three,
    S,
    Z,
}

impl From<manifest::OptLevel> for OptLevel {
    fn from(value: manifest::OptLevel) -> Self {
        match value {
            manifest::OptLevel::Zero => Self::Zero,
            manifest::OptLevel::One => Self::One,
            manifest::OptLevel::Two => Self::Two,
            manifest::OptLevel::Three => Self::Three,
            manifest::OptLevel::S => Self::S,
            manifest::OptLevel::Z => Self::Z,
        }
    }
}

impl Default for Profile {
    fn default() -> Self {
        Self {
            opt_level: Default::default(),
            dvm_bytecode: false,
            incremental: true,
            c_std: true,
        }
    }
}

macro_rules! determine_field {
    (
        function_name: $fun_name:ident,
        helper_function_name: $fun_name_help:ident,
        field_name: $name:ident,
        field_type: $ret:ty $(,)?
    ) => {
        #[doc = concat!("Detemine the [`", stringify!($name), "`] field of the profile `profile_name`\nNote:\n-----\nField in [`manifest::Profile`] should implement [`Into::into`] for the appropriate [`Profile`] field type.")]
        fn $fun_name(profile_name: StrId, profiles: &manifest::Profiles) -> QuackResult<$ret> {
            // The profile should either be defined in the manifest or be predefined.
            // We always prioritize the manifest, since a predefined profile can be redefined in the manifest.
            let Some(starting_profile) = profiles.get_profiles().get(&profile_name) else {
                return PREDEFINED_PROFILES
                    .get(&profile_name)
                    .map(|prof| prof.$name)
                    .ok_or_else(|| {
                        qp_err!("Unknown profile `{}`", profile_name).add_hint(
                            "In order to use a profile you have to define it in the manifest first",
                        )
                    });
            };
            $fun_name_help(starting_profile, profiles)
        }

        #[doc = concat!("Recursive helper for [`", stringify!($fun_name),"`]")]
        fn $fun_name_help(
            cur_profile: &manifest::Profile,
            profiles: &manifest::Profiles,
        ) -> QuackResult<$ret> {
            if let Some(result) = cur_profile.$name {
                // Manifest profile `cur_profile` has the field defined, so it overwrites all of its ancestors.
                // Since we have got here, none of the descendants defines this field.
                // Thus we can just return the value and not be concerned about the ancestors.
                return Ok(result.into());
            }
            if let Some(parent_name) = cur_profile.inherits {
                // Parent from which we inherit can be either defined in the manifest or predefined.
                // We always prioritize the manifest, since a predefined profile can be redefined in the manifest.
                if let Some(parent_profile) = profiles.get_profiles().get(&parent_name) {
                    // If it is defined in the manifest we recursively query the parent.
                    $fun_name_help(parent_profile, profiles)
                } else {
                    // Else we get the predefined profile and the appropriate field.
                    PREDEFINED_PROFILES
                        .get(&parent_name)
                        .map(|prof| prof.$name)
                        .context_internal("Parent profile neither in profiles map nor predefined")
                }
            } else {
                // None of the inheritance ancestors specified the field, so the default value should be used.
                Ok(Profile::default().$name)
            }
        }
    };
}

determine_field!(
    function_name: determine_dvm_bytecode,
    helper_function_name: determine_dvm_bytecode_rec,
    field_name: dvm_bytecode,
    field_type: bool,
);

determine_field!(
    function_name: determine_incremental,
    helper_function_name: determine_incremental_rec,
    field_name: incremental,
    field_type: bool,
);

determine_field!(
    function_name: determine_opt_level,
    helper_function_name: determine_opt_level_rec,
    field_name: opt_level,
    field_type: OptLevel,
);

determine_field!(
    function_name: determine_c_std,
    helper_function_name: determine_c_std_rec,
    field_name: c_std,
    field_type: bool,
);

impl Profile {
    pub fn construct_profile(
        profile_name: StrId,
        manifest_profiles: &manifest::Profiles,
    ) -> QuackResult<Self> {
        Ok(Self {
            opt_level: determine_opt_level(profile_name, manifest_profiles)?,
            dvm_bytecode: determine_dvm_bytecode(profile_name, manifest_profiles)?,
            incremental: determine_incremental(profile_name, manifest_profiles)?,
            c_std: determine_c_std(profile_name, manifest_profiles)?,
        })
    }
}

impl Display for OptLevel {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            OptLevel::Zero => write!(f, "0"),
            OptLevel::One => write!(f, "1"),
            OptLevel::Two => write!(f, "2"),
            OptLevel::Three => write!(f, "3"),
            OptLevel::S => write!(f, "s"),
            OptLevel::Z => write!(f, "z"),
        }
    }
}
