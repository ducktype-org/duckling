//! Home of
use std::fmt;

use super::util::filter_conditions;
use crate::quackpack::core::Manifest;
use crate::quackpack::core::lints::buffer::LintBuffer;
use crate::quackpack::core::lints::rules::util::MatchedConditions;
use crate::quackpack::core::lints::{Diagnostic, Lint};
use crate::{DuckContext, QuackResult, StrId};

pub const LINT: Lint = Lint {
    name: "always_false_conditions",
    description: r#" # Always false conditions
## What this lint does?

It checks for conditions which are always false.

## Why is this bad?

The dependency (or a dependency's feature) becomes effectively unused.

## Example

```yaml
dependencies:
  foo:
    version: 1.0.0
    conditions:
      package-features: []
```
"#,
};

/// Run the pass for [`LINT`].
pub fn pass(manifest: &Manifest, _: &DuckContext, buffer: &mut LintBuffer) -> QuackResult<()> {
    let filtered_conds = filter_conditions(manifest, |conds| {
        conds
            .required_root_package_features()
            .is_some_and(|slice| slice.is_empty())
    });
    for cond in filtered_conds {
        match cond {
            MatchedConditions::Dep { dep, _conds } => {
                let diag = DependencyWithEmptyConditionsDiagnostic { dep: dep.name() };
                buffer.register_warning(diag, LINT);
            }
            MatchedConditions::Feature {
                feature,
                dep,
                _conds,
            } => {
                let diag = DependencyFeatureWithEmptyConditionsDiagnostic {
                    dep: dep.name(),
                    feature: feature.name(),
                };
                buffer.register_warning(diag, LINT);
            }
        }
    }
    Ok(())
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
