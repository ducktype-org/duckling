//! Home of "nonexistent_features" lint.

use super::util::{nonexistent_features, walk_conditions};
use crate::quackpack::core::Manifest;
use crate::quackpack::core::lints::buffer::LintBuffer;
use crate::quackpack::core::lints::rules::util::MatchedConditions;
use crate::quackpack::core::lints::{Lint, LintLevel, macros};
use crate::{DuckContext, QuackResult, StrId};

pub const LINT: Lint = Lint {
    name: "nonexistent_features",
    description: r#"# Nonexistent features
## What this lint does?

It checks for conditions which depend on nonexistent features.

## Why is this bad?

This can cause a dependency to be always disabled.

## Example

```yaml
dependencies:
  foo:
    version: 1.0.0
    conditions:
      package-features: [nonexistent]
```
"#,
    level: LintLevel::Error,
};

/// Run the pass for [`LINT`].
pub fn pass(manifest: &Manifest, _: &DuckContext, buffer: &mut LintBuffer) -> QuackResult<()> {
    walk_conditions(manifest, |conds| {
        let Some(required_features) = conds.conds().required_root_package_features() else {
            return;
        };
        for nonexistent_feature in nonexistent_features(manifest, required_features) {
            match conds {
                MatchedConditions::Dep { dep, conds: _ } => {
                    let diag = DependencyConditionedOnNonexistentFeatureDiagnostic {
                        dep: dep.name(),
                        undeclared_feature_name: nonexistent_feature,
                    };
                    buffer.register_error(diag, LINT);
                }
                MatchedConditions::Feature {
                    feature,
                    dep,
                    conds: _,
                } => {
                    let diag = DependencyFeatureConditionedOnNonexistentFeatureDiagnostic {
                        dep: dep.name(),
                        feature: feature.name(),
                        undeclared_feature_name: nonexistent_feature,
                    };
                    buffer.register_error(diag, LINT);
                }
            }
        }
    });
    Ok(())
}

macros::make_diagnostic! {
    struct DependencyConditionedOnNonexistentFeatureDiagnostic {
        dep: StrId,
        undeclared_feature_name: StrId,
    }
    display(
       "dependency `{}` is conditioned on a nonexistent feature `{}`",
       dep, undeclared_feature_name
    )
}

macros::make_diagnostic! {
    struct DependencyFeatureConditionedOnNonexistentFeatureDiagnostic {
        dep: StrId,
        feature: StrId,
        undeclared_feature_name: StrId,
    }
    display(
       "feature `{}` of dependency `{}` is conditioned on a nonexistent feature `{}`",
       feature, dep, undeclared_feature_name
    )
}
