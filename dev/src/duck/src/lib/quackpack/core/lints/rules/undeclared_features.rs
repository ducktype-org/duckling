//! Home of "undeclared_features" lint.
use std::fmt;

use super::util::walk_conditions;
use crate::quackpack::core::lints::buffer::LintBuffer;
use crate::quackpack::core::lints::rules::util::MatchedConditions;
use crate::quackpack::core::lints::{Diagnostic, Lint};
use crate::quackpack::core::{FeatureName, Manifest};
use crate::{DuckContext, QuackResult, StrId};

pub const LINT: Lint = Lint {
    name: "undeclared_features",
    description: r#"# Undeclared features
## What this lint does?

It checks for conditions which depend on undeclared features.

## Why is this bad?

This can cause a dependency to be always disabled.

## Example

```yaml
dependencies:
  foo:
    version: 1.0.0
    conditions:
      package-features: [undeclared]
```
"#,
};

/// Run the pass for [`LINT`].
pub fn pass(manifest: &Manifest, _: &DuckContext, buffer: &mut LintBuffer) -> QuackResult<()> {
    walk_conditions(manifest, |conds| {
        let Some(required_features) = conds.conds().required_root_package_features() else {
            return;
        };
        for undeclared_feature in undeclared_features(manifest, required_features) {
            match conds {
                MatchedConditions::Dep { dep, conds: _ } => {
                    let diag = DependencyConditionedOnUndeclaredFeatureDiagnostic {
                        dep: dep.name(),
                        undeclared_feature_name: undeclared_feature,
                    };
                    buffer.register_warning(diag, LINT);
                }
                MatchedConditions::Feature {
                    feature,
                    dep,
                    conds: _,
                } => {
                    let diag = DependencyFeatureConditionedOnUndeclaredFeatureDiagnostic {
                        dep: dep.name(),
                        feature: feature.name(),
                        undeclared_feature_name: undeclared_feature,
                    };
                    buffer.register_warning(diag, LINT);
                }
            }
        }
    });
    Ok(())
}

fn undeclared_features<'a>(
    manifest: &'a Manifest,
    required_features: &'a [FeatureName],
) -> impl Iterator<Item = FeatureName> + 'a {
    required_features
        .iter()
        .copied()
        .filter(|feature| !manifest.features().has_feature(*feature))
}

#[derive(Debug)]
struct DependencyConditionedOnUndeclaredFeatureDiagnostic {
    dep: StrId,
    undeclared_feature_name: StrId,
}

#[derive(Debug)]
struct DependencyFeatureConditionedOnUndeclaredFeatureDiagnostic {
    dep: StrId,
    feature: StrId,
    undeclared_feature_name: StrId,
}

impl fmt::Display for DependencyConditionedOnUndeclaredFeatureDiagnostic {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "dependency `{}` is conditioned on an undeclared feature `{}`",
            self.dep, self.undeclared_feature_name
        )
    }
}

impl fmt::Display for DependencyFeatureConditionedOnUndeclaredFeatureDiagnostic {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "feature `{}` of dependency `{}` is conditioned on an undeclared feature `{}`",
            self.feature, self.dep, self.undeclared_feature_name
        )
    }
}

impl Diagnostic for DependencyFeatureConditionedOnUndeclaredFeatureDiagnostic {}
impl Diagnostic for DependencyConditionedOnUndeclaredFeatureDiagnostic {}
