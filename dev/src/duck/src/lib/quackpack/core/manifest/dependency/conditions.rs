use std::collections::HashSet;

use anyhow::bail;

use crate::{QuackResult, StrId, quackpack::core::FeatureName};

#[derive(Debug)]
/// Conditions required by a dependency or a feature flag in order to be enabled.
/// This is enabled for `any(system) and any(arch) and any(flags)`.
pub struct Conditions {
    /// Required operating systems for this condition.
    system_requirements: Option<Vec<StrId>>,
    /// Required architectures for this condition.
    arch_requirements: Option<Vec<StrId>>,
    /// Required root package features for this condition.
    required_root_package_features: Option<Vec<FeatureName>>,
}

impl Conditions {
    /// Create new conditions with validation.
    ///
    /// Fails if any of the optional vectors are present but empty.
    pub fn new(
        system_requirements: Option<Vec<StrId>>,
        arch_requirements: Option<Vec<StrId>>,
        required_root_package_features: Option<Vec<FeatureName>>,
    ) -> QuackResult<Self> {
        fn check_non_empty(t: &Option<Vec<StrId>>, name: &'static str) -> QuackResult<()> {
            if let Some(vec) = t
                && vec.is_empty()
            {
                bail!(
                    "the field `{name}` is present but empty, if you don't want to specify it, remove it from the manifest"
                )
            }
            Ok(())
        }

        check_non_empty(&system_requirements, "system")?;
        check_non_empty(&arch_requirements, "arch")?;
        check_non_empty(&required_root_package_features, "package_features")?;
        Ok(Self {
            system_requirements,
            arch_requirements,
            required_root_package_features,
        })
    }

    /// Check if conditions are met for the given enabled features.
    /// This checks `any(system) and any(arch) and any(flags)`.
    // @TODO: #1353 Do we want to take an `impl IntoIterator`, or a `Vec`, or a `HashSet`?
    //  Connected with !TODO in `are_features_enabled`.
    pub fn is_enabled_for(&self, enabled_features: impl IntoIterator<Item = FeatureName>) -> bool {
        self.is_system_enabled()
            && self.is_arch_enabled()
            && self.are_features_enabled(enabled_features)
    }

    fn is_system_enabled(&self) -> bool {
        let Some(ref _systems) = self.system_requirements else {
            return true;
        };
        // @TODO: #1353 implement
        true
    }

    fn is_arch_enabled(&self) -> bool {
        let Some(ref _arches) = self.arch_requirements else {
            return true;
        };
        // @TODO: #1353 implement
        true
    }

    fn are_features_enabled(
        &self,
        enabled_features: impl IntoIterator<Item = FeatureName>,
    ) -> bool {
        let Some(ref features) = self.required_root_package_features else {
            return true;
        };
        let enabled_features = enabled_features.into_iter().collect::<HashSet<_>>();
        let required_features = HashSet::from_iter(features.iter().copied());
        // NOTE: Leaving this as-is, because it's easier to parse.
        // @TODO: #1353 We could work with plain iterators and/or keep `required_root_package_features` as a HashSet,
        //  but only if creating temporary HashSets becomes a bottleneck. Also connected with !TODO above, in `is_enabled_for`.
        !enabled_features.is_disjoint(&required_features)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn conditions_new() {
        let result = Conditions::new(None, None, None);
        assert!(result.is_ok());
        let system_reqs = Some(vec![StrId::new("linux"), StrId::new("macos")]);
        let arch_reqs = Some(vec![StrId::new("x86_64"), StrId::new("aarch64")]);
        let features = Some(vec![StrId::new("a"), StrId::new("b")]);

        let result = Conditions::new(system_reqs, arch_reqs, features);
        assert!(result.is_ok());

        let result = Conditions::new(Some(vec![]), None, None);
        assert!(result.is_err());
        let err = result.unwrap_err();
        assert_eq!(
            err.to_string(),
            "the field `system` is present but empty, if you don't want to specify it, remove it from the manifest"
        );

        let result = Conditions::new(None, Some(vec![]), None);
        assert_eq!(
            result.unwrap_err().to_string(),
            "the field `arch` is present but empty, if you don't want to specify it, remove it from the manifest"
        );

        let result = Conditions::new(None, None, Some(vec![]));
        assert_eq!(
            result.unwrap_err().to_string(),
            "the field `package_features` is present but empty, if you don't want to specify it, remove it from the manifest"
        );
    }

    #[test]
    // @TODO: #1353 When we begin to check host system, add also that.
    //  We will probably need to do some conditional logic (make sure it runs on CI!).
    fn enabled_conditions() {
        let empty_condition = Conditions::new(None, None, None).unwrap();
        assert!(empty_condition.is_enabled_for(vec![]));
        assert!(empty_condition.is_enabled_for(vec![StrId::new("a")]));

        let a_b_condition =
            Conditions::new(None, None, Some(vec![StrId::new("a"), StrId::new("b")])).unwrap();

        assert!(a_b_condition.is_enabled_for(vec![StrId::new("a"), StrId::new("c")]));

        assert!(!a_b_condition.is_enabled_for(vec![StrId::new("c"), StrId::new("d")]));

        assert!(a_b_condition.is_enabled_for(vec![
            StrId::new("a"),
            StrId::new("b"),
            StrId::new("c"),
        ]));

        let a_condition = Conditions::new(None, None, Some(vec![StrId::new("a")])).unwrap();

        assert!(!a_condition.is_enabled_for(vec![]));

        assert!(a_condition.is_enabled_for(vec![StrId::new("a")]));

        assert!(!a_condition.is_enabled_for(vec![StrId::new("b")]));
    }
}
