use std::collections::HashMap;
use std::fmt::Display;
use std::sync::LazyLock;

use tracing::debug;

use crate::quackpack::core::manifest;
use crate::util::error::MessageError;
use crate::{QuackError, QuackResult, QuackResultContext, StrId};

pub static PREDEFINED_PROFILES: LazyLock<HashMap<StrId, Profile>> = LazyLock::new(|| {
    [
        (
            "dev".into(),
            Profile {
                name: "dev".into(),
                opt_level: OptLevel::One,
                dvm_bytecode: false,
                incremental: true,
                c_std: true,
            },
        ),
        (
            "release".into(),
            Profile {
                name: "release".into(),
                opt_level: OptLevel::Three,
                dvm_bytecode: false,
                incremental: false,
                c_std: true,
            },
        ),
        (
            "test".into(),
            Profile {
                name: "test".into(),
                opt_level: OptLevel::One,
                dvm_bytecode: false,
                incremental: true,
                c_std: true,
            },
        ),
        (
            "bench".into(),
            Profile {
                name: "bench".into(),
                opt_level: OptLevel::Three,
                dvm_bytecode: false,
                incremental: false,
                c_std: true,
            },
        ),
        (
            DEFAULT_SCRIPT_PROFILE_NAME.into(),
            Profile {
                name: DEFAULT_SCRIPT_PROFILE_NAME.into(),
                opt_level: OptLevel::Three,
                dvm_bytecode: true,
                incremental: true, // This does not effect how the script is compiled, but still affects the dependencies.
                c_std: true,
            },
        ),
    ]
    .into()
});

pub static DEFAULT_PROFILE: LazyLock<Profile> = LazyLock::new(Profile::default);
pub const DEFAULT_SCRIPT_PROFILE_NAME: &str = "script";
pub static DEFAULT_SCRIPT_PROFILE: LazyLock<Profile> = LazyLock::new(|| {
    *PREDEFINED_PROFILES
        .get(&DEFAULT_SCRIPT_PROFILE_NAME.into())
        .unwrap()
});

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
/// List of specific options which should be passed to the compiler.
pub struct Profile {
    /// Profile's name.
    pub name: StrId,
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

#[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
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
            name: "default".into(),
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
        #[doc = concat!("Determine the [`", stringify!($name), "`] field of the profile `profile_name`")]
        ///
        /// Note:
        /// -----
        /// Field in [`manifest::Profile`] should implement [`Into::into`] for the appropriate [`Profile`] field type.
        /// It should also implement [`std::marker::Copy`].
        #[tracing::instrument(skip_all)]
        fn $fun_name(profile_name: StrId, profiles: &manifest::Profiles) -> QuackResult<$ret> {
            debug!(profile = %profile_name, ?profiles, "getting a profile from manifest");
            // The profile should be either defined in the manifest or predefined.
            // We always prioritize the manifest, since a predefined profile can be redefined in the manifest.
            let Some(starting_profile) = profiles.get_profiles().get(&profile_name) else {
                return PREDEFINED_PROFILES
                    .get(&profile_name)
                    .map(|prof| prof.$name)
                    .ok_or_else(|| {
                        QuackError::hint(
                            "In order to use a profile you have to define it in the manifest first",
                        )
                        .context(MessageError(
                            format!("Unknown profile `{}`", profile_name).into(),
                        ))
                    });
            };
            $fun_name_help(profile_name, starting_profile, profiles)
        }

        #[doc = concat!("Recursive helper for [`", stringify!($fun_name),"`]")]
        #[tracing::instrument(skip_all)]
        fn $fun_name_help(
            cur_profile_name: StrId,
            cur_profile: &manifest::Profile,
            profiles: &manifest::Profiles,
        ) -> QuackResult<$ret> {
            debug!(profile.name = %cur_profile_name, profile = ?cur_profile, ?profiles, "getting a profile from manifest");
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
                    $fun_name_help(parent_name, parent_profile, profiles)
                } else {
                    // Else we get the predefined profile and the appropriate field.
                    PREDEFINED_PROFILES
                        .get(&parent_name)
                        .map(|prof| prof.$name)
                        .context_internal("Parent profile neither in profiles map nor predefined")
                }
            } else {
                // None of the inheritance ancestors specified the field.
                // If the last ancestor overwrites some predefined profile, we inherit from it.
                // Otherwise we use the default value.
                if let Some(predefined) = PREDEFINED_PROFILES.get(&cur_profile_name) {
                    Ok(predefined.$name)
                } else {
                    Ok(Profile::default().$name)
                }
            }
        }
    };
}

determine_field!(
    function_name: determine_dvm_bytecode,
    helper_function_name: get_dvm_bytecode_or_ask_parent,
    field_name: dvm_bytecode,
    field_type: bool,
);

determine_field!(
    function_name: determine_incremental,
    helper_function_name: get_incremental_or_ask_parent,
    field_name: incremental,
    field_type: bool,
);

determine_field!(
    function_name: determine_opt_level,
    helper_function_name: get_opt_level_or_ask_parent,
    field_name: opt_level,
    field_type: OptLevel,
);

determine_field!(
    function_name: determine_c_std,
    helper_function_name: get_c_std_or_ask_parent,
    field_name: c_std,
    field_type: bool,
);

impl Profile {
    /// Constructs a [`Profile`], given the profile's name and [`manifest::Profiles`].
    /// Unwinds the inheritance structure to determine each field of the profile.
    #[tracing::instrument(skip_all)]
    pub fn construct_profile(
        profile_name: StrId,
        manifest_profiles: &manifest::Profiles,
    ) -> QuackResult<Self> {
        debug!(profile = %profile_name, profiles = ?manifest_profiles, "getting a profile from manifest");
        Ok(Self {
            name: profile_name,
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

#[cfg(test)]
mod test {
    use super::*;

    /// Profile `b` inherits from `release_with_s`, which inherits from `release`.
    /// Profile `b` should be equal to:
    /// ```Profile {
    ///     opt_level: OptLevel::S, // inherited from release_with_s
    ///     dvm_bytecode: false, // inherited from release
    ///     incremental: false, // inherited from release
    ///     c_std: false, // defined in b
    /// }```
    #[test]
    fn inherit_defined_predefined() {
        let profile_release_with_s = manifest::Profile {
            opt_level: Some(manifest::OptLevel::S),
            dvm_bytecode: None,
            incremental: None,
            c_std: None,
            inherits: Some("release".into()),
        };
        let profile_b = manifest::Profile {
            opt_level: None,
            dvm_bytecode: None,
            incremental: None,
            c_std: Some(false),
            inherits: Some("release_with_s".into()),
        };
        let profiles = manifest::Profiles::new(
            [
                ("release_with_s".into(), profile_release_with_s),
                ("b".into(), profile_b),
            ]
            .into(),
        )
        .unwrap();
        let profile = Profile::construct_profile("b".into(), &profiles).unwrap();
        assert_eq!(
            profile,
            Profile {
                name: "b".into(),
                opt_level: OptLevel::S,
                dvm_bytecode: false,
                incremental: false,
                c_std: false,
            },
        )
    }

    #[test]
    fn default_fields() {
        let profile = manifest::Profile {
            opt_level: None,
            dvm_bytecode: None,
            incremental: None,
            c_std: None,
            inherits: None,
        };
        let profiles = manifest::Profiles::new([("default".into(), profile)].into()).unwrap();
        let profile = Profile::construct_profile("default".into(), &profiles).unwrap();
        assert_eq!(profile, Profile::default());
    }

    #[test]
    fn unknown_profile() {
        let profiles = manifest::Profiles::new([].into()).unwrap();
        let error = Profile::construct_profile("a".into(), &profiles).unwrap_err();
        assert_eq!(
            error.to_string(),
            "Unknown profile `a`\nIn order to use a profile you have to define it in the manifest first",
        )
    }

    #[test]
    fn overwriting_predefined() {
        let profile = manifest::Profile {
            opt_level: None,
            dvm_bytecode: None,
            incremental: Some(false),
            c_std: None,
            inherits: None,
        };
        let profiles = manifest::Profiles::new([("dev".into(), profile)].into()).unwrap();
        let profile = Profile::construct_profile("dev".into(), &profiles).unwrap();
        assert_eq!(
            profile,
            Profile {
                name: "dev".into(),
                opt_level: OptLevel::One,
                dvm_bytecode: false,
                incremental: false,
                c_std: true,
            }
        );
    }

    #[test]
    fn inheritance_from_overwritten_predefined() {
        let dev = manifest::Profile {
            opt_level: None,
            dvm_bytecode: None,
            incremental: Some(false),
            c_std: None,
            inherits: None,
        };
        let a = manifest::Profile {
            opt_level: None,
            dvm_bytecode: None,
            incremental: None,
            c_std: Some(false),
            inherits: Some("dev".into()),
        };
        let profiles =
            manifest::Profiles::new([("dev".into(), dev), ("a".into(), a)].into()).unwrap();
        let profile = Profile::construct_profile("a".into(), &profiles).unwrap();
        assert_eq!(
            profile,
            Profile {
                name: "a".into(),
                opt_level: OptLevel::One,
                dvm_bytecode: false,
                incremental: false,
                c_std: false,
            }
        );
    }
}
