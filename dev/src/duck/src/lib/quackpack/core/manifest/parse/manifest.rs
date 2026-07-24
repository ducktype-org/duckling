//! Parsing of the manifest from its schema.
use std::collections::HashMap;
use std::path::Path;

use tracing::debug;

use super::{Scope, dependency};
use crate::quackpack::core::valid_package_name::ValidPackageName;
use crate::quackpack::core::{
    Features, Manifest, OptLevel, PackageMetadata, ParseMode, Profile, Profiles, ScopeGuard,
    Version,
};
use crate::quackpack::schemas::manifest::{
    Manifest as ManifestSchema, OptLevel as SchemaOptLevel, Profile as ProfileSchema,
};
use crate::util::IsPlural;
use crate::util::hash::sha256_string;
use crate::{DuckContext, QuackResult, QuackResultContext, StrId, qp_bail, qp_err};

/// Parse [`Manifest`] from given [`ManifestSchema`].
#[tracing::instrument(skip_all)]
pub(crate) fn parse(
    schema: &ManifestSchema,
    root: &Path,
    mode: ParseMode,
    ctx: &DuckContext,
) -> QuackResult<Manifest> {
    let mut scope = Scope::new();
    let guard = scope.push("dependencies".into());
    let dependencies = dependency::parse(schema.dependencies.as_ref(), root, ctx, guard)?;

    let guard = scope.push("dev-dependencies".into());
    let dev_dependencies = dependency::parse(schema.dev_dependencies.as_ref(), root, ctx, guard)?;

    let guard = scope.push("profiles".into());
    let profiles = parse_profiles(schema.profiles.as_ref(), guard)?;
    match mode {
        ParseMode::FrontMatterScript => {
            let illegal_fields = schema.fields_disallowed_in_expanded_frontmatter();
            if !illegal_fields.is_empty() {
                let mut err = qp_err!(
                    "illegal field{} `{}` in the frontmatter at {}",
                    illegal_fields.s_if_plural(),
                    illegal_fields.join("`, `"),
                    root.display()
                );
                err = err.add_hint(
                    "remove all the fields besides `dependencies`, `dev-dependencies` and `profiles`",
                );
                qp_bail!(err);
            }

            let name = root.file_stem().unwrap().display().to_string();

            ValidPackageName::new(name.as_str()).with_context(|| {
                format!("script at `{}` has an invalid script name", root.display())
            })?;

            let name = StrId::from(format!(
                "{}-{}",
                name,
                sha256_string(root.as_os_str().as_encoded_bytes())
            ));
            let version = Version::default();
            let manifest = Manifest::new(
                name,
                version,
                Features::default(),
                PackageMetadata::default(),
                dependencies,
                dev_dependencies,
                profiles,
            );
            Ok(manifest)
        }
        ParseMode::Package => {
            if schema.import.is_some() {
                qp_bail!("`import` field is prohibited in manifests")
            }
            let Some(ref metadata) = schema.metadata else {
                qp_bail!("missing the obligatory section `metadata`")
            };
            let Some(version) = metadata.version else {
                qp_bail!("missing the obligatory key `metadata.version`")
            };
            let Some(ref name) = metadata.name else {
                qp_bail!("missing the obligatory key `metadata.name`")
            };
            debug!("package name is `{name}`, version is `{version}`");

            {
                let mut guard1 = scope.push("metadata".to_string());
                let guard2 = guard1.push("name".to_string());

                let _ = ValidPackageName::new(name.as_str())
                    .context("package has an invalid name")
                    .with_context(|| guard2.make_context_string())?;
            }
            let guard = scope.push("features".into());
            let features = parse_features(schema.features.as_ref())
                .with_context(move || guard.make_context_string())?;
            let authors = metadata
                .authors
                .as_ref()
                .map(|vec| vec.iter().map(<&String>::into).collect())
                .unwrap_or_default();
            let package_metadata = PackageMetadata::new(
                authors,
                metadata.license.as_ref().map(<&String>::into),
                metadata.description.as_ref().map(<&String>::into),
            );

            Ok(Manifest::new(
                name.into(),
                version,
                features,
                package_metadata,
                dependencies,
                dev_dependencies,
                profiles,
            ))
        }
    }
}

/// Parse [`Features`] from the given features map.
fn parse_features(features: Option<&HashMap<String, Vec<String>>>) -> QuackResult<Features> {
    let Some(features) = features else {
        return Features::new(HashMap::new());
    };
    let as_hash_map = features
        .iter()
        .map(|(k, v)| (k.into(), v.iter().map(<&String>::into).collect()))
        .collect();
    Features::new(as_hash_map)
}

/// Parse [`Profiles`] from the given compiler flags mapping.
fn parse_profiles(
    input: Option<&HashMap<String, ProfileSchema>>,
    mut scope: ScopeGuard<'_>,
) -> QuackResult<Profiles> {
    let Some(input) = input else {
        return Ok(Profiles::default());
    };
    (|| {
        let profiles_map: QuackResult<HashMap<StrId, Profile>> = input
            .iter()
            .map(|(k, v)| {
                let guard = scope.push(k.into());
                (parse_profile(v))
                    .map(|new_v| (k.into(), new_v))
                    .with_context(move || guard.make_context_string())
            })
            .collect();
        Profiles::new(profiles_map?)
    })()
    .with_context(move || scope.make_context_string())
}

fn parse_profile(input: &ProfileSchema) -> QuackResult<Profile> {
    let opt_level = if let Some(schema_opt_level) = &input.opt_level {
        Some(match &schema_opt_level {
            SchemaOptLevel::Number(0) => OptLevel::Zero,
            SchemaOptLevel::Number(1) => OptLevel::One,
            SchemaOptLevel::Number(2) => OptLevel::Two,
            SchemaOptLevel::Number(3) => OptLevel::Three,
            SchemaOptLevel::Number(n) => qp_bail!(
                "Unknown optimization level `{n}`. Optimization levels are 0, 1, 2, 3, s (or S), z (or Z)."
            ),
            SchemaOptLevel::String(str) => match str.as_ref() {
                "s" => OptLevel::S,
                "S" => OptLevel::S,
                "z" => OptLevel::Z,
                "Z" => OptLevel::Z,
                "0" => OptLevel::Zero,
                "1" => OptLevel::One,
                "2" => OptLevel::Two,
                "3" => OptLevel::Three,
                str => qp_bail!(
                    "Unknown optimization level `{str}`. Optimization levels are 0, 1, 2, 3, s (or S), z (or Z)."
                ),
            },
        })
    } else {
        None
    };
    let dvm_bytecode = input.dvm_bytecode;
    let incremental = input.incremental;
    let c_std = input.c_std;
    let inherits = input.inherits.clone().map(Into::into);
    Ok(Profile {
        opt_level,
        dvm_bytecode,
        incremental,
        c_std,
        inherits,
    })
}
