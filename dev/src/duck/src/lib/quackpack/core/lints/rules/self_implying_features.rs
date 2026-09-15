//! Home of "feature pulls itself" lint.

use crate::quackpack::core::Manifest;
use crate::quackpack::core::lints::buffer::LintBuffer;
use crate::quackpack::core::lints::{Lint, LintLevel, macros};
use crate::{DuckContext, QuackResult, StrId};

pub const LINT: Lint = Lint {
    name: "self_implying_features",
    description: r#"# Self implying features
## What this lint does?

It checks for features which imply (pull) themselves.

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
            buffer.register_warning(SelfImplyingFeatureDiagnostic { feature: *feature }, LINT);
        }
    }
    Ok(())
}

macros::make_diagnostic! {
    struct SelfImplyingFeatureDiagnostic {
        feature: StrId,
    }
    display(
        "feature `{}` implies itself", feature
    )
}
