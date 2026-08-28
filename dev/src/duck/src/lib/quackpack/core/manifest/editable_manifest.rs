use std::path::Path;

use yaml_edit::{Mapping, YamlFile, YamlNode};

use crate::quackpack::core::{DependencyKind, Package, PackageContext};
use crate::quackpack::schemas::manifest::Dependency;
use crate::util::DescriptionWithAnArticle;
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QuackResult, QuackResultContext, qp_bail};

/// Desired order of the top-level keys in a manifest.
const TOPLEVEL_KEYS_ORDER: [&str; 6] = [
    "metadata",
    "dependencies",
    "dev-dependencies",
    "features",
    "profiles",
    "venv",
];

impl DescriptionWithAnArticle for YamlNode {
    fn desc_with_article(&self) -> &'static str {
        match self {
            Self::Scalar(..) => "a value",
            Self::Sequence(..) => "an array",
            Self::Mapping(..) => "a table",
            Self::TaggedNode(..) => "a tagged value",
            Self::Alias(..) => "an alias",
        }
    }
}

/// Type allowing lossless manipulation of the manifest.
/// Lossless here means for example retaining comments present in the manifest or user's stylistic choises.
pub struct EditableManifest<'duck> {
    ctx: &'duck DuckContext,
    package: &'duck Package,
    yaml_file: YamlFile,
}

/// Marker struct for a removal of a dependency.
pub enum DependencyRemoved {
    /// Dependency successfully removed.
    Yes,
    /// No such dependency found.
    NoDependency,
    /// No such dependency found for the given kind.
    /// However there is a dependency with this name but of different kind.
    NoDependencyButKindExists(DependencyKind),
}

/// Marker struct for an addition of a dependency.
pub enum DependencyAdded {
    /// Dependency successfully added.
    Yes,
    /// Dependency with such name and kind already exists.
    AlreadyExists,
}

impl<'duck> EditableManifest<'duck> {
    /// Create a new [`EditableManifest`].
    pub fn new(pcx: &'duck PackageContext) -> QuackResult<Self> {
        let ctx = pcx.ctx();
        let pkg = pcx.package().get_package();
        let content = YamlFile::from_path(pkg.manifest_path()).with_context(|| {
            format!(
                "when trying to editably read the manifest at `{}`",
                pkg.manifest_path().display()
            )
        })?;
        let result = Self {
            ctx,
            package: pkg,
            yaml_file: content,
        };
        // Make sure `as_mapping` will work in the future.
        result.as_mapping()?;
        Ok(result)
    }

    /// Get the path of the manifest of this [`EditableManifest`].
    fn manifest_path(&self) -> &Path {
        self.package.manifest_path()
    }

    /// Get the underlying top level [`Mapping`] of this [`EditableManifest`].
    fn as_mapping(&self) -> QuackResult<Mapping> {
        if self.yaml_file.documents().count() > 1 {
            qp_bail!(
                "manifest file `{}` contains multiple YAML documents which is not supported",
                self.manifest_path().display()
            )
        }
        self.yaml_file
            .document()
            .with_context(|| {
                format!(
                    "no yaml document found inside `{}`",
                    self.manifest_path().display()
                )
            })?
            .as_mapping()
            .with_context(|| {
                format!(
                    "document found inside `{}` is not a mapping",
                    self.manifest_path().display()
                )
            })
    }

    /// Remove a dependency from the manifest.
    pub fn remove_dependency(
        &self,
        name: &str,
        kind: DependencyKind,
    ) -> QuackResult<DependencyRemoved> {
        let manifest = self.as_mapping()?;
        let dependencies_maps = [
            (DependencyKind::Normal, manifest.get("dependencies")),
            (DependencyKind::Dev, manifest.get("dev-dependencies")),
        ];
        let mut other_kind = None;
        let mut removed = false;
        // We iterate over all kinds of dependencies for better error messages.
        for (dep_kind, dep_map) in dependencies_maps {
            if dep_kind == kind {
                // Remove the dependency.
                if let Some(dep_map) = dep_map {
                    let dep_map = self.dependencies_as_mapping(dep_map, dep_kind)?;
                    removed = dep_map.remove(name).is_some();
                    // If the mapping became empty, remove it from the manifest.
                    if dep_map.is_empty() {
                        manifest.remove(dep_kind.key_in_manifest());
                    }
                }
            } else if let Some(dep_map) = dep_map {
                // Check if there is a dependency of same name but different kind.
                let dep_map = self.dependencies_as_mapping(dep_map, dep_kind)?;
                if dep_map.contains_key(name) {
                    other_kind = Some(dep_kind);
                }
            }
        }
        if removed {
            self.save()?;
            Ok(DependencyRemoved::Yes)
        } else if let Some(other_kind) = other_kind {
            Ok(DependencyRemoved::NoDependencyButKindExists(other_kind))
        } else {
            Ok(DependencyRemoved::NoDependency)
        }
    }

    /// Add a dependency into the manifest.
    pub fn add_dependency(
        &self,
        name: &str,
        dependency: Dependency,
        kind: DependencyKind,
    ) -> QuackResult<DependencyAdded> {
        let dependencies_map = self.get_dependencies_map(kind)?;
        if dependencies_map.contains_key(name) {
            return Ok(DependencyAdded::AlreadyExists);
        } else {
            let dependency = Into::<Mapping>::into(dependency);
            dependencies_map.set(name, dependency);
        }
        self.save()?;
        Ok(DependencyAdded::Yes)
    }

    /// Get a [`Mapping`] of the dependencies of the given kind.
    /// If a mapping for the given kind is absent, inserts an empty one.
    fn get_dependencies_map(&self, kind: DependencyKind) -> QuackResult<Mapping> {
        let manifest = self.as_mapping()?;
        let key = kind.key_in_manifest();
        if !manifest.contains_key(key) {
            manifest.set_with_field_order(key, Mapping::new(), TOPLEVEL_KEYS_ORDER);
        }
        let dependencies = manifest.get(key).with_context_internal(|| {
            format!("inserted key {key} but there is no entry for it in {manifest:?}")
        })?;
        self.dependencies_as_mapping(dependencies, kind)
    }

    /// Cast a [`YamlNode`] representing an entry for dependencies into a [`Mapping`] or generate a meaningful error.
    fn dependencies_as_mapping(
        &self,
        mapping: YamlNode,
        kind: DependencyKind,
    ) -> QuackResult<Mapping> {
        let YamlNode::Mapping(mapping) = mapping else {
            qp_bail!(
                "in the manifest at `{}`, `{}` should be a mapping, not {}",
                self.manifest_path().display(),
                kind.key_in_manifest(),
                mapping.desc_with_article(),
            )
        };
        Ok(mapping)
    }

    /// Serialize the underlying [`YamlFile`] to the manifest.
    fn save(&self) -> QuackResult<()> {
        let new_manifest = self.yaml_file.to_string();
        self.manifest_path().write(new_manifest).with_context(|| {
            format!(
                "failed to write the new manifest into file at `{}`",
                self.manifest_path().display()
            )
        })?;
        self.ctx.console().info(format!(
            "written new manifest to `{}`",
            self.manifest_path().display()
        ))
    }
}
