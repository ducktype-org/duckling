// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Home of "unknown_lints" lint.

use std::fmt;

use crate::QuackResult;
use crate::quackpack::core::PackageContext;
use crate::quackpack::core::lints::buffer::LintBuffer;
use crate::quackpack::core::lints::{Diagnostic, Lint, LintLevel};
use crate::util::edit_distance::find_closest_target;

pub const LINT: Lint = Lint {
    name: "unknown_lints",
    description: r#"# Unknown lints
## What this lint does?

It checks for misconfigured lints in `lints.duck` mapping in the manifest.

## Why is this bad?

It could be a typo.

## Example

```yaml
lints:
  duck:
    this_is_a_typO: warning
```
"#,
    level: LintLevel::Warning,
};

/// Run the pass for [`LINT`].
pub fn pass(pcx: &PackageContext<'_>, buffer: &mut LintBuffer) -> QuackResult<()> {
    let level = pcx.package().manifest().lints().level_for_duck_lint(&LINT);
    if level == LintLevel::Allow {
        return Ok(());
    }
    let Some(ref lints) = pcx.package().original_schema().lints else {
        return Ok(());
    };
    let Some(ref duck) = lints.duck else {
        return Ok(());
    };
    let known_lints = known_lint_names();
    for lint_name in duck.0.keys() {
        if is_known_lint(lint_name) {
            continue;
        }
        let hint = find_closest_target(lint_name, &known_lints, None).map(ToString::to_string);
        buffer.register(
            UnknownLintDiagnostic {
                lint_name: lint_name.to_string(),
                hint,
            },
            LINT,
            level,
        );
    }
    Ok(())
}

fn known_lint_names() -> Vec<&'static str> {
    super::ALL_LINTS.iter().map(|lint| lint.name).collect()
}

fn is_known_lint(name: &str) -> bool {
    super::ALL_LINTS.iter().any(|lint| lint.name == name)
}

#[derive(Debug)]
struct UnknownLintDiagnostic {
    lint_name: String,
    hint: Option<String>,
}

impl fmt::Display for UnknownLintDiagnostic {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "there is no such lint as `{}`", self.lint_name)?;
        if let Some(ref hint) = self.hint {
            writeln!(f)?;
            write!(f, "hint: did you mean `{}`", hint)?;
        }
        Ok(())
    }
}
impl Diagnostic for UnknownLintDiagnostic {}
