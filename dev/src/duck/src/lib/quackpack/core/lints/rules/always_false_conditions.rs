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
use crate::quackpack::core::{DependencyKind, FeatureName, Manifest};
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
    level: LintLevel::Error,
};

/// Run the pass for [`LINT`].
pub fn pass(
    manifest: &Manifest,
    _: &DuckContext,
    level: LintLevel,
    buffer: &mut LintBuffer,
) -> QuackResult<()> {
    walk_conditions(manifest, |conds| {
        let Some(required_features) = conds.conds().required_root_package_features() else {
            return;
        };
        if required_features.is_empty() {
            return emit_empty_features_diag(conds, buffer, level);
        }
        if has_only_nonexistent_features(required_features, manifest) {
            emit_only_nonexistent_features_diag(conds, buffer, level);
        }
    });
    Ok(())
}

fn emit_empty_features_diag(
    conds: MatchedConditions<'_>,
    buffer: &mut LintBuffer,
    level: LintLevel,
) {
    match conds {
        MatchedConditions::Dep { dep, conds: _ } => {
            let diag = DependencyWithEmptyConditionsDiagnostic {
                dep_kind: dep.kind(),
                dep: dep.diagnostic_name(),
            };
            buffer.register(diag, LINT, level);
        }
        MatchedConditions::Feature {
            feature,
            dep,
            conds: _,
        } => {
            let diag = DependencyFeatureWithEmptyConditionsDiagnostic {
                dep_kind: dep.kind(),
                dep: dep.diagnostic_name(),
                feature: feature.name(),
            };
            buffer.register(diag, LINT, level);
        }
    }
}

fn has_only_nonexistent_features(required_features: &[FeatureName], manifest: &Manifest) -> bool {
    required_features
        .iter()
        .all(|feature| is_nonexistent_feature(manifest, *feature))
}

fn emit_only_nonexistent_features_diag(
    conds: MatchedConditions<'_>,
    buffer: &mut LintBuffer,
    level: LintLevel,
) {
    match conds {
        MatchedConditions::Dep { dep, conds: _ } => {
            let diag = DependencyWithOnlyNonexistentFeaturesDiagnostic {
                dep_kind: dep.kind(),
                dep: dep.diagnostic_name(),
            };
            buffer.register(diag, LINT, level);
        }
        MatchedConditions::Feature {
            feature,
            dep,
            conds: _,
        } => {
            let diag = DependencyFeatureWithOnlyNonexistentFeaturesDiagnostic {
                dep_kind: dep.kind(),
                dep: dep.diagnostic_name(),
                feature: feature.name(),
            };
            buffer.register(diag, LINT, level);
        }
    }
}

macros::make_diagnostic! {
    struct DependencyWithEmptyConditionsDiagnostic {
        dep_kind: DependencyKind,
        dep: StrId,
    }
    display(
       "{} dependency named `{}` is disabled, because it is conditioned on an empty features list",
       dep_kind, dep
    )
}

macros::make_diagnostic! {
    struct DependencyFeatureWithEmptyConditionsDiagnostic {
        dep_kind: DependencyKind,
        dep: StrId,
        feature: StrId,
    }
    display(
       "feature `{}` of {} dependency named `{}` is disabled, because it is conditioned on an empty features list",
       feature, dep_kind, dep
    )
}

macros::make_diagnostic! {
    struct DependencyWithOnlyNonexistentFeaturesDiagnostic {
        dep_kind: DependencyKind,
        dep: StrId,
    }
    display(
        "{} dependency named `{}` is disabled, because it is conditioned only on nonexistent features",
        dep_kind, dep
    )
}

macros::make_diagnostic! {
struct DependencyFeatureWithOnlyNonexistentFeaturesDiagnostic {
    dep_kind: DependencyKind,
    dep: StrId,
    feature: StrId,
}
    display(
        "feature `{}` of {} dependency named `{}` is disabled, because it is conditioned only on nonexistent features",
        feature, dep_kind, dep
    )
}
