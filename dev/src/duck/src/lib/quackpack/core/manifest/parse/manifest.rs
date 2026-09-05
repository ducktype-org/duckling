//! Parsing of the manifest from its schema.
use std::collections::HashMap;
use std::path::Path;

use tracing::debug;

use super::source::resolve_path_maybe_relative_to_dir;
use super::{Scope, dependency};
use crate::quackpack::core::lints::warnings::Warnings;
use crate::quackpack::core::manifest::VenvConfig;
use crate::quackpack::core::valid_package_name::validate_package_name;
use crate::quackpack::core::{
    BuildOptions, Dependencies, DependencyKind, Features, Manifest, OptLevel, PackageMetadata,
    ParseMode, Profile, Profiles, ScopeGuard, Version,
};
use crate::quackpack::schemas::manifest::{
    Manifest as ManifestSchema, Metadata as MetadataSchema, OptLevel as SchemaOptLevel,
    Profile as ProfileSchema, VenvConfig as VenvConfigSchema,
};
use crate::util::Pluralize;
use crate::{DuckContext, QuackResult, QuackResultContext, StrId, qp_bail, qp_err};

/// Parse [`Manifest`] from given [`ManifestSchema`].
#[tracing::instrument(skip_all)]
#[track_caller]
pub(crate) fn parse(
    schema: &ManifestSchema,
    root: &Path,
    mode: ParseMode,
    warnings: &mut Warnings,
    ctx: &DuckContext,
) -> QuackResult<Manifest> {
    let mut scope = Scope::new();
    let mut deps = Vec::new();
    let guard = scope.push("dependencies".into());
    dependency::parse(
        schema.dependencies.as_ref(),
        root,
        DependencyKind::Normal,
        &mut deps,
        warnings,
        ctx,
        guard,
    )?;

    let guard = scope.push("dev-dependencies".into());
    dependency::parse(
        schema.dev_dependencies.as_ref(),
        root,
        DependencyKind::Dev,
        &mut deps,
        warnings,
        ctx,
        guard,
    )?;
    let dependencies = Dependencies::new(deps)?;

    let guard = scope.push("profiles".into());
    let profiles = parse_profiles(schema.profiles.as_ref(), guard)?;
    match mode {
        ParseMode::FrontMatter => {
            let illegal_fields = schema.fields_disallowed_in_expanded_frontmatter();
            if !illegal_fields.is_empty() {
                let mut err = qp_err!(
                    "illegal field{} `{}` in the frontmatter at `{}`",
                    illegal_fields.s_if_plural(),
                    illegal_fields.join("`, `"),
                    root.display()
                );
                err = err.add_hint(
                    "remove all the fields besides `dependencies`, `dev-dependencies` and `profiles`",
                );
                qp_bail!(err);
            }

            // NOTE: `script.rs::FrontMatter::name` relies on the fact that script name ==
            // manifest.name.
            let name = StrId::from(root.file_stem().unwrap());

            validate_package_name(&name).with_context(|| {
                format!(
                    "script at `{}` has an invalid script name (file stem)",
                    root.display()
                )
            })?;

            let version = Version::default();
            let manifest = Manifest::new(
                name,
                version,
                Features::default(),
                PackageMetadata::default(),
                dependencies,
                profiles,
                VenvConfig::default_for_script(ctx),
                BuildOptions::default(),
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
            debug!(package_name = %name, package_version = %version);

            {
                let mut guard1 = scope.push("metadata".to_string());
                let guard2 = guard1.push("name".to_string());

                validate_package_name(name)
                    .context("package has an invalid name")
                    .with_context(|| guard2.make_context_string())?;
            }
            let guard = scope.push("features".into());
            let features = parse_features(schema.features.as_ref())
                .with_context(move || guard.make_context_string())?;
            let venv = parse_venv(schema.venv.as_ref(), root, ctx);
            let authors = metadata
                .authors
                .as_ref()
                .map(|vec| vec.iter().map(<&String>::into).collect())
                .unwrap_or_default();
            let package_metadata = PackageMetadata {
                authors,
                license: metadata.license.as_ref().map(<&String>::into),
                description: metadata.description.as_ref().map(<&String>::into),
            };

            let build_options = parse_build_options(metadata);

            Ok(Manifest::new(
                name.into(),
                version,
                features,
                package_metadata,
                dependencies,
                profiles,
                venv,
                build_options,
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

/// Parse a `venv:` field.
fn parse_venv(input: Option<&VenvConfigSchema>, root: &Path, ctx: &DuckContext) -> VenvConfig {
    let default_config = VenvConfig::default_for_package(ctx);
    let Some(input) = input else {
        return default_config;
    };
    let storage_root = if let Some(ref storage) = input.storage_path {
        resolve_path_maybe_relative_to_dir(storage, root, ctx)
    } else {
        default_config.storage_path().to_path_buf()
    };
    let expose_freezefile = input
        .expose_freezefile
        .unwrap_or(default_config.expose_freezefile());
    let ephemeral = input.ephemeral.unwrap_or(default_config.ephemeral());
    VenvConfig::new(storage_root, expose_freezefile, ephemeral)
}

/// Parse a [`BuildOptions`] from the schema.
fn parse_build_options(input: &MetadataSchema) -> BuildOptions {
    let links = input.links.as_ref().map(StrId::from);
    BuildOptions { links }
}
