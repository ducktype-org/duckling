//! Home of "always_false_conditions" lint.
use std::fmt;

use super::util::{is_nonexistent_feature, walk_conditions};
use crate::quackpack::core::lints::buffer::LintBuffer;
use crate::quackpack::core::lints::rules::util::MatchedConditions;
use crate::quackpack::core::lints::{Diagnostic, Lint, LintLevel};
use crate::quackpack::core::{FeatureName, Manifest};
use crate::{DuckContext, QuackResult, StrId};

pub const LINT: Lint = Lint {
    name: "always_false_conditions",
    description: r#"# Always false conditions
## What this lint does?

It checks for conditions which are always false.

## Why is this bad?

A dependency (or a dependency's feature) becomes effectively always disabled.

## Example

```yaml
dependencies:
  foo:
    version: 1.0.0
    conditions:
      package-features: []
```
"#,
    level: LintLevel::Warning,
};

/// Run the pass for [`LINT`].
pub fn pass(manifest: &Manifest, _: &DuckContext, buffer: &mut LintBuffer) -> QuackResult<()> {
    walk_conditions(manifest, |conds| {
        let Some(required_features) = conds.conds().required_root_package_features() else {
            return;
        };
        if required_features.is_empty() {
            return emit_empty_features_diag(conds, buffer);
        }
        if has_only_nonexistent_features(required_features, manifest) {
            emit_only_nonexistent_features_diag(conds, buffer);
        }
    });
    Ok(())
}

fn emit_empty_features_diag(conds: MatchedConditions<'_>, buffer: &mut LintBuffer) {
    match conds {
        MatchedConditions::Dep { dep, conds: _ } => {
            let diag = DependencyWithEmptyConditionsDiagnostic { dep: dep.name() };
            buffer.register_warning(diag, LINT);
        }
        MatchedConditions::Feature {
            feature,
            dep,
            conds: _,
        } => {
            let diag = DependencyFeatureWithEmptyConditionsDiagnostic {
                dep: dep.name(),
                feature: feature.name(),
            };
            buffer.register_warning(diag, LINT);
        }
    }
}

fn has_only_nonexistent_features(required_features: &[FeatureName], manifest: &Manifest) -> bool {
    required_features
        .iter()
        .all(|feature| is_nonexistent_feature(manifest, *feature))
}

fn emit_only_nonexistent_features_diag(conds: MatchedConditions<'_>, buffer: &mut LintBuffer) {
    match conds {
        MatchedConditions::Dep { dep, conds: _ } => {
            let diag = DependencyWithOnlyNonexistentFeaturesDiagnostic { dep: dep.name() };
            buffer.register_warning(diag, LINT);
        }
        MatchedConditions::Feature {
            feature,
            dep,
            conds: _,
        } => {
            let diag = DependencyFeatureWithOnlyNonexistentFeaturesDiagnostic {
                dep: dep.name(),
                feature: feature.name(),
            };
            buffer.register_warning(diag, LINT);
        }
    }
}

#[derive(Debug)]
struct DependencyWithEmptyConditionsDiagnostic {
    dep: StrId,
}

#[derive(Debug)]
struct DependencyFeatureWithEmptyConditionsDiagnostic {
    dep: StrId,
    feature: StrId,
}

impl fmt::Display for DependencyWithEmptyConditionsDiagnostic {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "dependency `{}` is disabled, because it is conditioned on an empty features list",
            self.dep
        )
    }
}

impl fmt::Display for DependencyFeatureWithEmptyConditionsDiagnostic {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "feature `{}` of dependency `{}` is disabled, because it is conditioned on an empty features list",
            self.feature, self.dep
        )
    }
}

impl Diagnostic for DependencyFeatureWithEmptyConditionsDiagnostic {}
impl Diagnostic for DependencyWithEmptyConditionsDiagnostic {}

#[derive(Debug)]
struct DependencyWithOnlyNonexistentFeaturesDiagnostic {
    dep: StrId,
}

#[derive(Debug)]
struct DependencyFeatureWithOnlyNonexistentFeaturesDiagnostic {
    dep: StrId,
    feature: StrId,
}

impl fmt::Display for DependencyWithOnlyNonexistentFeaturesDiagnostic {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "dependency `{}` is disabled, because it is conditioned only on nonexistent features",
            self.dep
        )
    }
}

impl fmt::Display for DependencyFeatureWithOnlyNonexistentFeaturesDiagnostic {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "feature `{}` of dependency `{}` is disabled, because it is conditioned only on nonexistent features",
            self.feature, self.dep
        )
    }
}

impl Diagnostic for DependencyFeatureWithOnlyNonexistentFeaturesDiagnostic {}
impl Diagnostic for DependencyWithOnlyNonexistentFeaturesDiagnostic {}
