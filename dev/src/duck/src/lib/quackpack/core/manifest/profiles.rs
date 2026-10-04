use std::collections::{HashMap, HashSet};

use crate::quackpack::core::compile::profiles::PREDEFINED_PROFILES;
use crate::quackpack::schemas::registry;
use crate::quackpack::util::str_id::QpJoin;
use crate::{QuackError, QuackResult, StrId, qp_bail, qp_bail_internal, qp_err};

#[derive(Clone, Debug, PartialEq, Eq)]
/// List of specific options which should be passed to the compiler.
/// All fields are wrapped in [`Option`],
/// since this is a tight abstraction over the real, yaml manifest entry.
/// Expansion of inheritance and providing defaults is done in the
/// [`compile::profiles`](crate::quackpack::core::compile::profiles) module.
pub struct Profile {
    pub opt_level: Option<OptLevel>,
    pub dvm_bytecode: Option<bool>,
    pub incremental: Option<bool>,
    pub c_std: Option<bool>,
    pub inherits: Option<StrId>,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
/// Enum for different possible optimization levels in the compiler.
pub enum OptLevel {
    Zero,
    One,
    Two,
    Three,
    S,
    Z,
}

impl From<registry::Profile> for Profile {
    fn from(value: registry::Profile) -> Self {
        Self {
            opt_level: value.opt_level.map(Into::into),
            dvm_bytecode: value.dvm_bytecode,
            incremental: value.incremental,
            c_std: value.c_std,
            inherits: value.inherits.map(Into::into),
        }
    }
}

impl From<Profile> for registry::Profile {
    fn from(value: Profile) -> Self {
        registry::Profile {
            opt_level: value.opt_level.map(Into::into),
            dvm_bytecode: value.dvm_bytecode,
            incremental: value.incremental,
            c_std: value.c_std,
            inherits: value.inherits.map(Into::into),
        }
    }
}

#[derive(Clone, Debug, Default)]
/// Struct representing different user-defined profiles.
pub struct Profiles(HashMap<StrId, Profile>);

impl Profiles {
    /// Get the underlying mapping from profile names to profiles.
    pub fn get_profiles(&self) -> &HashMap<StrId, Profile> {
        &self.0
    }

    /// Creates a new [`Profiles`] instance from a given map and checks its validity.
    pub fn new(profiles: HashMap<StrId, Profile>) -> QuackResult<Self> {
        let result = Self(profiles);
        result.check_inheritance_parents_exist()?;
        result.check_no_inheritance_cycle()?;
        Ok(result)
    }

    /// Check that if some profile inherits from another, the latter is also defined.
    fn check_inheritance_parents_exist(&self) -> QuackResult<()> {
        for (profile_name, profile) in self.0.iter() {
            let Some(parent) = profile.inherits else {
                continue;
            };
            if !self.0.contains_key(&parent) && !PREDEFINED_PROFILES.contains_key(&parent) {
                qp_bail!(
                    "Profile `{profile_name}` inherits from a non-existent profile `{parent}`"
                );
            }
        }
        Ok(())
    }

    /// Check that there is no cycle of inheritances.
    /// This is done by visiting profiles in an iterative manner.
    /// If a profile is yet unvisited, we start a dfs-like procedure from it,
    /// transitioning to the profile it inherits from.
    fn check_no_inheritance_cycle(&self) -> QuackResult<()> {
        let mut visited_profiles: HashSet<StrId> = HashSet::new();
        for predefined_profile in PREDEFINED_PROFILES.keys() {
            if !self.0.contains_key(predefined_profile) {
                visited_profiles.insert(*predefined_profile);
            }
        }
        for profile_name in self.0.keys() {
            if visited_profiles.contains(profile_name) {
                continue;
            }
            let mut current_visit = HashMap::new();
            self.check_cycle_from_one_profile(
                *profile_name,
                &visited_profiles,
                &mut current_visit,
                0,
            )?;
            visited_profiles.extend(current_visit.into_keys());
        }
        Ok(())
    }

    /// Procedure exploring the inheritance graph from a given profile.
    /// Profiles visited in this procedure are stored in *current_visit*,
    /// while profiles visited in previous procedures are stored in *previously_visited*.
    /// To be able to both efficiently check whether a cycle is encountered and
    /// to create a verbose error message, we keep track of profiles visited in this procedure,
    /// by a mapping from profile names to the index of their visits.
    fn check_cycle_from_one_profile(
        &self,
        profile_name: StrId,
        previously_visited: &HashSet<StrId>,
        current_visit: &mut HashMap<StrId, usize>,
        counter: usize,
    ) -> QuackResult<()> {
        let Some(profile) = self.0.get(&profile_name) else {
            qp_bail_internal!(
                "no profile `{profile_name}`, but already checked, that it exists? {self:#?}"
            );
        };
        if let Some(previous_occurrence) = current_visit.get(&profile_name) {
            return Err(create_cycle_error_msg(
                current_visit,
                *previous_occurrence,
                counter,
                profile_name,
            ));
        }
        current_visit.insert(profile_name, counter);
        if let Some(parent) = profile.inherits
            && !previously_visited.contains(&parent)
        {
            self.check_cycle_from_one_profile(
                parent,
                previously_visited,
                current_visit,
                counter + 1,
            )?;
        }
        Ok(())
    }
}

/// Creates a verbose error message after encountering a cycle,
/// by extracting the cycle from the subgraph of profiles visited in the current procedure.
fn create_cycle_error_msg(
    visit: &HashMap<StrId, usize>,
    previous_occurrence: usize,
    counter: usize,
    cycling_profile: StrId,
) -> QuackError {
    let mut visit_ord = vec![StrId::new(""); counter - previous_occurrence];
    for (visited_profile, i) in visit.iter() {
        if *i >= previous_occurrence {
            visit_ord[*i - previous_occurrence] = *visited_profile;
        }
    }
    qp_err!(
        "Inheritance in user-defined profiles cycles: {} <- {cycling_profile}",
        visit_ord.join(" <- ")
    )
}

impl From<HashMap<String, registry::Profile>> for Profiles {
    fn from(value: HashMap<String, registry::Profile>) -> Self {
        Self(
            value
                .into_iter()
                .map(|(k, v)| (k.into(), v.into()))
                .collect(),
        )
    }
}

impl From<Profiles> for HashMap<String, registry::Profile> {
    fn from(value: Profiles) -> Self {
        value
            .0
            .into_iter()
            .map(|(k, v)| (k.into(), v.into()))
            .collect()
    }
}

impl From<OptLevel> for registry::OptLevel {
    fn from(value: OptLevel) -> Self {
        match value {
            OptLevel::Zero => registry::OptLevel::Zero,
            OptLevel::One => registry::OptLevel::One,
            OptLevel::Two => registry::OptLevel::Two,
            OptLevel::Three => registry::OptLevel::Three,
            OptLevel::S => registry::OptLevel::S,
            OptLevel::Z => registry::OptLevel::Z,
        }
    }
}

impl From<registry::OptLevel> for OptLevel {
    fn from(value: registry::OptLevel) -> Self {
        match value {
            registry::OptLevel::Zero => OptLevel::Zero,
            registry::OptLevel::One => OptLevel::One,
            registry::OptLevel::Two => OptLevel::Two,
            registry::OptLevel::Three => OptLevel::Three,
            registry::OptLevel::S => OptLevel::S,
            registry::OptLevel::Z => OptLevel::Z,
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn profiles_inheritance_cycle() {
        let prof_a = Profile {
            opt_level: Some(OptLevel::S),
            dvm_bytecode: Some(false),
            incremental: Some(false),
            c_std: Some(false),
            inherits: Some("b".into()),
        };
        let prof_b = Profile {
            opt_level: Some(OptLevel::S),
            dvm_bytecode: Some(false),
            incremental: Some(false),
            c_std: Some(false),
            inherits: Some("c".into()),
        };
        let prof_c = Profile {
            opt_level: Some(OptLevel::S),
            dvm_bytecode: Some(false),
            incremental: Some(false),
            c_std: Some(false),
            inherits: Some("d".into()),
        };
        let prof_d = Profile {
            opt_level: Some(OptLevel::S),
            dvm_bytecode: Some(false),
            incremental: Some(false),
            c_std: Some(false),
            inherits: Some("b".into()),
        };
        let profiles_map = [
            ("a".into(), prof_a),
            ("b".into(), prof_b),
            ("c".into(), prof_c),
            ("d".into(), prof_d),
        ]
        .into();
        let err = Profiles::new(profiles_map).unwrap_err();
        let possible_error_msgs = [
            "Inheritance in user-defined profiles cycles: b <- c <- d <- b",
            "Inheritance in user-defined profiles cycles: c <- d <- b <- c",
            "Inheritance in user-defined profiles cycles: d <- b <- c <- d",
        ];
        assert!(possible_error_msgs.contains(&err.to_string().as_str()));
    }

    #[test]
    fn profile_inheritance_self_cycle() {
        let prof_a = Profile {
            opt_level: Some(OptLevel::S),
            dvm_bytecode: Some(false),
            incremental: Some(false),
            c_std: Some(false),
            inherits: Some("a".into()),
        };
        let profiles_map = [("a".into(), prof_a)].into();
        let err = Profiles::new(profiles_map).unwrap_err();
        assert!(err.to_string() == "Inheritance in user-defined profiles cycles: a <- a");
    }

    #[test]
    fn inheritance_from_predefined() {
        let prof_a = Profile {
            opt_level: Some(OptLevel::S),
            dvm_bytecode: Some(false),
            incremental: Some(false),
            c_std: Some(false),
            inherits: Some("dev".into()),
        };
        let profiles_map = [("a".into(), prof_a)].into();
        Profiles::new(profiles_map).unwrap();
    }

    #[test]
    fn cycle_with_overwritten_predefined() {
        let prof_a = Profile {
            opt_level: Some(OptLevel::S),
            dvm_bytecode: Some(false),
            incremental: Some(false),
            c_std: Some(false),
            inherits: Some("dev".into()),
        };
        let prof_dev = Profile {
            opt_level: Some(OptLevel::S),
            dvm_bytecode: Some(false),
            incremental: Some(false),
            c_std: Some(false),
            inherits: Some("a".into()),
        };
        let profiles_map = [("a".into(), prof_a), ("dev".into(), prof_dev)].into();
        let err = Profiles::new(profiles_map).unwrap_err();
        let possible_error_msgs = [
            "Inheritance in user-defined profiles cycles: a <- dev <- a",
            "Inheritance in user-defined profiles cycles: dev <- a <- dev",
        ];
        assert!(possible_error_msgs.contains(&err.to_string().as_str()));
    }

    #[test]
    fn inheritance_from_nonexistent_profile() {
        let prof_a = Profile {
            opt_level: Some(OptLevel::S),
            dvm_bytecode: Some(false),
            incremental: Some(false),
            c_std: Some(false),
            inherits: Some("b".into()),
        };
        let profiles_map = [("a".into(), prof_a)].into();
        let err = Profiles::new(profiles_map).unwrap_err();
        assert!(err.to_string() == "Profile `a` inherits from a non-existent profile `b`");
    }
}
