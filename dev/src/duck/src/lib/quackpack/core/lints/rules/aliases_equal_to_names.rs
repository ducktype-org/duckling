//! Home of "aliases equal to names" lint.
use std::fmt;

use crate::QuackResult;
use crate::quackpack::core::lints::buffer::LintBuffer;
use crate::quackpack::core::lints::{Diagnostic, Lint, LintLevel};
use crate::quackpack::core::{DependencyKind, PackageContext};
use crate::quackpack::schemas::manifest::DependencySource;

pub const LINT: Lint = Lint {
    name: "aliases_equal_to_names",
    description: r#"# Aliases equal to names
## What this lint does?

It checks for dependencies which are aliased,
but the dependency's name (specified in `source`) is equal to the alias.

## Why is this bad?

It is redundant.

## Example

```yaml
dependencies:
  foo:
    source:
      path: ../foo
      name: foo
```

## Corrected example

```yaml
dependencies:
  foo:
    source:
      path: ../foo
```
"#,
    level: LintLevel::Warning,
};

/// Run the pass for [`LINT`].
pub fn pass(pcx: &PackageContext<'_>, buffer: &mut LintBuffer) -> QuackResult<()> {
    let schema = pcx.package().original_schema();
    let dependencies_maps = [
        (DependencyKind::Normal, schema.dependencies.as_ref()),
        (DependencyKind::Dev, schema.dev_dependencies.as_ref()),
    ];
    for (dep_kind, dependencies) in dependencies_maps {
        for (effective_name, dependency) in dependencies.into_iter().flatten() {
            if let Some(ref source) = dependency.source
                && let DependencySource::Detailed(source) = source
                && let Some(ref name) = source.name
                && effective_name == name
            {
                buffer.register_warning(
                    AliasEqualsNameDiagnostic {
                        name: name.clone(),
                        dep_kind,
                    },
                    LINT,
                );
            }
        }
    }
    Ok(())
}

#[derive(Debug)]
struct AliasEqualsNameDiagnostic {
    name: String,
    dep_kind: DependencyKind,
}

impl fmt::Display for AliasEqualsNameDiagnostic {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{} dependency's alias `{}` is equal to its name specified at `{}.{}.source.name`",
            self.dep_kind,
            self.name,
            self.dep_kind.key_in_manifest(),
            self.name
        )
    }
}

impl Diagnostic for AliasEqualsNameDiagnostic {}
