// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Home of "nonexistent_features" lint.

use std::collections::HashMap;

use crate::quackpack::core::lints::buffer::LintBuffer;
use crate::quackpack::core::lints::{Lint, LintLevel, macros};
use crate::quackpack::core::{Dependency, DependencyKind, Manifest, SourceKind};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::{DuckContext, QuackResult, StrId};

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
/// Version of [`SourceKind`], disregarding [`GitReference`](crate::quackpack::core::manifest) for [`SourceKind::Git`],
/// but taking into account names for registry dependencies.
enum SimpleSourceKind {
    Registry { name: StrId },
    Local,
    Git,
}

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
/// Version of [`Source`](crate::quackpack::core::manifest), disregarding [`GitReference`](crate::quackpack::core::manifest) for git sources.
/// Should be used for checking whether there are two dependencies in the manifest with the same source.
/// By same source, we mean:
/// - same registry and name for registry dependencies,
/// - same path for local dependencies,
/// - same git url (but not necessarily reference) for git dependencies.
pub struct UniqueSource {
    kind: SimpleSourceKind,
    url: InternedUrl,
}

impl From<&Dependency> for UniqueSource {
    fn from(dependency: &Dependency) -> Self {
        let kind = match dependency.source().kind() {
            SourceKind::Registry => SimpleSourceKind::Registry {
                name: dependency.name(),
            },
            SourceKind::Local => SimpleSourceKind::Local,
            SourceKind::Git(_) => SimpleSourceKind::Git,
        };
        let url = dependency.source().url();
        UniqueSource { kind, url }
    }
}

pub const LINT: Lint = Lint {
    name: "same sources",
    description: r#"# Nonexistent features
## What this lint does?

It checks for dependencies with the same source (even for dependencies with different kinds).

## Why is this bad?

This can introduce weird bugs due to conflicts between same-source packages.
Also objects from same-source packages seem interchangable, but are not.

## Example

```yaml
dependencies:
  foo:
    version: 1.0.0
dev-dependencies:
  foo:
    version: 2.0.0
```
"#,
    level: LintLevel::Error,
};

/// Run the pass for [`LINT`].
pub fn pass(manifest: &Manifest, _: &DuckContext, buffer: &mut LintBuffer) -> QuackResult<()> {
    let level = LINT.level;
    if level == LintLevel::Allow {
        return Ok(());
    }
    // Mapping from sources to pairs (<effective name>, <dependency kind>).
    let mut dependencies_sources: HashMap<UniqueSource, (StrId, DependencyKind)> = HashMap::new();
    for dep in manifest.dependencies().all_dependencies() {
        let unique_source = dep.into();
        if let Some((other_dep_name, other_dep_kind)) = dependencies_sources.get(&unique_source) {
            buffer.register(
                SameSourcesDiagnostic {
                    dep1_name: dep.effective_name(),
                    dep1_kind: dep.kind(),
                    dep2_name: *other_dep_name,
                    dep2_kind: *other_dep_kind,
                },
                LINT,
                level,
            );
        } else {
            dependencies_sources.insert(unique_source, (dep.effective_name(), dep.kind()));
        }
    }
    Ok(())
}

macros::make_diagnostic! {
    struct SameSourcesDiagnostic {
        dep1_name: StrId,
        dep1_kind: DependencyKind,
        dep2_name: StrId,
        dep2_kind: DependencyKind,
    }
    display(
        "{} dependency `{}` has the same source as {} dependency `{}`", dep1_kind, dep1_name, dep2_kind, dep2_name
    )
}
