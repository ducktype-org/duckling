//! Home of "feature pulls itself" lint.
use std::fmt;

use crate::quackpack::core::Manifest;
use crate::quackpack::core::lints::buffer::LintBuffer;
use crate::quackpack::core::lints::{Diagnostic, Lint, LintLevel};
use crate::{DuckContext, QuackResult, StrId};

pub const LINT: Lint = Lint {
    name: "feature_pulls_itself",
    description: r#"# Feature pulls itself
## What this lint does?

It checks for features which pull (imply) themselves.

## Why is this bad?

It is redundant.

## Example

```yaml
features:
  foo: [foo]
```

## Corrected example

```yaml
features:
  foo: []
```
"#,
    level: LintLevel::Warning,
};

/// Run the pass for [`LINT`].
pub fn pass(manifest: &Manifest, _: &DuckContext, buffer: &mut LintBuffer) -> QuackResult<()> {
    for (feature, expands_to) in manifest.features().all_features() {
        if expands_to.contains(feature) {
            buffer.register_warning(FeaturePullsItselfDiagnostic { feature: *feature }, LINT);
        }
    }
    Ok(())
}

#[derive(Debug)]
struct FeaturePullsItselfDiagnostic {
    feature: StrId,
}

impl fmt::Display for FeaturePullsItselfDiagnostic {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "feature `{}` pulls itself", self.feature)
    }
}

impl Diagnostic for FeaturePullsItselfDiagnostic {}