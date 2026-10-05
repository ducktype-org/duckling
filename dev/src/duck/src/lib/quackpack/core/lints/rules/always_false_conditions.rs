// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Home of "always_false_conditions" lint.

use super::util::{is_nonexistent_feature, walk_conditions};
use crate::quackpack::core::lints::buffer::LintBuffer;
use crate::quackpack::core::lints::rules::util::MatchedConditions;
use crate::quackpack::core::lints::{Lint, LintLevel, macros};
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

macros::make_diagnostic! {
    struct DependencyWithEmptyConditionsDiagnostic {
        dep: StrId,
    }
    display(
       "dependency `{}` is disabled, because it is conditioned on an empty features list",
       dep
    )
}

macros::make_diagnostic! {
    struct DependencyFeatureWithEmptyConditionsDiagnostic {
        dep: StrId,
        feature: StrId,
    }
    display(
       "feature `{}` of dependency `{}` is disabled, because it is conditioned on an empty features list",
       feature, dep
    )
}

macros::make_diagnostic! {
    struct DependencyWithOnlyNonexistentFeaturesDiagnostic {
        dep: StrId,
    }
    display(
        "dependency `{}` is disabled, because it is conditioned only on nonexistent features",
        dep
    )
}

macros::make_diagnostic! {
struct DependencyFeatureWithOnlyNonexistentFeaturesDiagnostic {
    dep: StrId,
    feature: StrId,
}
    display(
        "feature `{}` of dependency `{}` is disabled, because it is conditioned only on nonexistent features",
        feature, dep
    )
}
