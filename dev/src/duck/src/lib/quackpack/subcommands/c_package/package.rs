// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Layout of a C package on disk, and the parts of its manifest that `duck c-package` writes.

use std::fs;
use std::path::{Path, PathBuf};
use std::str::FromStr;

use serde::Deserialize;
use yaml_edit::{Sequence, YamlFile, YamlNode};

use super::resolve::Resolved;
use crate::{QuackResult, QuackResultContext, qp_bail};

/// Directory under `src/` owned by `duck c-package regen`.
pub const GENERATED_DIR: &str = "generated";

/// Paths inside a C package rooted at `root`.
pub struct PackagePaths {
    pub root: PathBuf,
}

impl PackagePaths {
    pub fn manifest(&self) -> PathBuf {
        self.root.join("quackconfig.yaml")
    }

    pub fn src(&self) -> PathBuf {
        self.root.join("src")
    }

    pub fn generated(&self) -> PathBuf {
        self.src().join(GENERATED_DIR)
    }
}

#[derive(Deserialize)]
struct NameOnly {
    metadata: NameOnlyMetadata,
}

#[derive(Deserialize)]
struct NameOnlyMetadata {
    name: String,
}

/// The package's name, from its manifest.
pub fn read_name(paths: &PackagePaths) -> QuackResult<String> {
    let manifest = paths.manifest();
    let text = fs::read_to_string(&manifest)
        .with_context(|| format!("failed to read `{}`", manifest.display()))?;
    let parsed: NameOnly = serde_yaml_ng::from_str(&text)
        .with_context(|| format!("`{}` has no `metadata.name`", manifest.display()))?;
    Ok(parsed.metadata.name)
}

/// Writes the manifest of a new package.
pub fn write_new_manifest(
    paths: &PackagePaths,
    name: &str,
    version: &str,
    resolved: &Resolved,
) -> QuackResult<()> {
    let mut text = format!("metadata:\n  name: {name}\n  version: '{version}'\n");
    for (key, values) in library_keys(resolved) {
        if !values.is_empty() {
            text += &format!("  {key}: [{}]\n", values.join(", "));
        }
    }
    write(&paths.manifest(), &text)
}

/// Rewrites only `metadata.external-libs` and `metadata.external-lib-paths`, keeping the rest of
/// the manifest, comments included.
pub fn update_manifest_libraries(paths: &PackagePaths, resolved: &Resolved) -> QuackResult<()> {
    let manifest = paths.manifest();
    let text = fs::read_to_string(&manifest)
        .with_context(|| format!("failed to read `{}`", manifest.display()))?;
    let file = YamlFile::from_str(&text)
        .with_context(|| format!("failed to parse `{}`", manifest.display()))?;
    let Some(YamlNode::Mapping(metadata)) = file
        .document()
        .and_then(|document| document.as_mapping())
        .and_then(|root| root.get("metadata"))
    else {
        qp_bail!("`{}` has no `metadata` table", manifest.display());
    };
    for (key, values) in library_keys(resolved) {
        if values.is_empty() {
            metadata.remove(key);
            continue;
        }
        let sequence = Sequence::new_flow();
        for value in values {
            sequence.push(value.as_str());
        }
        metadata.set(key, sequence);
    }
    write(&manifest, &file.to_string())
}

fn library_keys(resolved: &Resolved) -> [(&'static str, &Vec<String>); 2] {
    [
        ("external-libs", &resolved.external_libs),
        ("external-lib-paths", &resolved.external_lib_paths),
    ]
}

/// Writes `src/src.dk` re-exporting the bindings, unless the package already has one: it belongs
/// to the user from then on.
pub fn write_root_module_if_missing(paths: &PackagePaths, name: &str) -> QuackResult<()> {
    let root_module = paths.src().join("src.dk");
    if root_module.exists() {
        return Ok(());
    }
    write(
        &root_module,
        &format!(
            "# Re-exports the generated bindings, so `import {name}.*;` reaches them. This file is \
             never\n# regenerated: helpers written here or in other files next to it are kept.\n\
             import {name}.{GENERATED_DIR}.{name}.*;\n"
        ),
    )
}

/// The main module of `src/generated/`, without which the directory is not a module.
pub fn generated_root_module(banner: &[String]) -> String {
    banner.iter().map(|line| format!("# {line}\n")).collect()
}

pub fn write(path: &Path, content: &str) -> QuackResult<()> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)
            .with_context(|| format!("failed to create `{}`", parent.display()))?;
    }
    fs::write(path, content).with_context(|| format!("failed to write `{}`", path.display()))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn resolved(libs: &[&str], paths: &[&str]) -> Resolved {
        Resolved {
            external_libs: libs.iter().map(|&lib| lib.to_owned()).collect(),
            external_lib_paths: paths.iter().map(|&path| path.to_owned()).collect(),
            ..Default::default()
        }
    }

    #[test]
    fn new_manifest() {
        let dir = tempfile::tempdir().unwrap();
        let paths = PackagePaths {
            root: dir.path().to_path_buf(),
        };
        write_new_manifest(&paths, "sdl3", "3.4.16", &resolved(&["SDL3"], &[])).unwrap();
        assert_eq!(
            fs::read_to_string(paths.manifest()).unwrap(),
            "metadata:\n  name: sdl3\n  version: '3.4.16'\n  external-libs: [SDL3]\n"
        );
        assert_eq!(read_name(&paths).unwrap(), "sdl3");
    }

    #[test]
    fn update_keeps_everything_else() {
        let dir = tempfile::tempdir().unwrap();
        let paths = PackagePaths {
            root: dir.path().to_path_buf(),
        };
        write(
            &paths.manifest(),
            "# mine\nmetadata:\n  name: z\n  version: '1.3.0'\n  external-libs: [old]\n  \
             external-lib-paths: [gone]\n  description: zlib bindings # keep\n\
             dependencies:\n  other: '1'\n",
        )
        .unwrap();
        update_manifest_libraries(&paths, &resolved(&["z", "m"], &[])).unwrap();
        let text = fs::read_to_string(paths.manifest()).unwrap();
        assert!(text.starts_with("# mine\n"), "{text}");
        assert!(text.contains("external-libs: [z, m]"), "{text}");
        assert!(!text.contains("external-lib-paths"), "{text}");
        assert!(text.contains("description: zlib bindings # keep"), "{text}");
        assert!(text.contains("dependencies:\n  other: '1'"), "{text}");
    }
}
