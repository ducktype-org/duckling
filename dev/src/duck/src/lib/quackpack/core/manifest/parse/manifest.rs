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

/// Get a single atribute from an object.
macro_rules! get_single_key {
    ($object:ident, $key:ident) => {{ $object.$key.as_ref() }};
}

/// Helper for [`get_key`].
macro_rules! _get_key {
    ($object:ident, $key:ident, $($previous:ident).*) => {{
        get_single_key!($object, $key).with_context(|| {
            let previous_list: Vec<&str> = vec![$(stringify!($previous)).*];
            let previous_list_str = if previous_list.is_empty() { String::new() } else {
                format!("{}.", previous_list.join("."))
            };
            format!("missing the obligatory key `{previous_list_str}{}`", stringify!($key))
        })
    }};
    ($object:ident, $first:ident . $($rest:ident).*, $($previous:ident).*) => {{
        let Some(object) = get_single_key!($object, $first) else {
            let previous_list: Vec<&str> = vec![$(stringify!($previous)).*];
            let previous_list_str = if previous_list.is_empty() { String::new() } else {
                format!("{}.", previous_list.join("."))
            };
            qp_bail!("missing the obligatory key `{previous_list_str}{}`", stringify!($first))
        };
        _get_key!(object, $($rest)*, $($previous:ident).* $first)
    }}
}

/// Get a nested atribute from an object.
/// If at any point encounters [`Option::None`], bails.
macro_rules! get_key {
    ($object:ident, $key:ident) => {{
        _get_key!($object, $key, )
    }};
    ($object:ident, $first:ident . $($rest:ident).*) => {{
        _get_key!($object, $first . $($rest).*, )
    }}
}

/// Parse [`Manifest`] from given [`ManifestSchema`].
//#[tracing::instrument(skip_all)]
#[track_caller]
pub(crate) fn parse(
    schema: &ManifestSchema,
    root: &Path,
    mode: ParseMode,
    warnings: &mut Warnings,
    ctx: &DuckContext,
) -> QuackResult<Manifest> {
    check_illegal_fields(schema, &mode, root)?;
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
            let metadata = get_key!(schema, metadata)?;
            let name = get_key!(schema, metadata.name)?;
            let version = *get_key!(schema, metadata.version)?;
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
            let authors = get_key!(schema, metadata.authors)
                .cloned()
                .unwrap_or_default();
            let package_metadata = PackageMetadata {
                authors,
                license: get_key!(schema, metadata.license).ok().cloned(),
                description: get_key!(schema, metadata.description).ok().cloned(),
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

/// Check that the [`ManifestSchema`] does not contain any fields disallowed in this [`ParseMode`].
fn check_illegal_fields(schema: &ManifestSchema, mode: &ParseMode, root: &Path) -> QuackResult<()> {
    match mode {
        ParseMode::Package => {
            if get_single_key!(schema, import).is_some() {
                qp_bail!(
                    "illegal field `import` in the manifest at `{}`",
                    root.display()
                )
            }
            Ok(())
        }
        ParseMode::FrontMatter => {
            let mut found_illegal_fields = vec![];
            if get_single_key!(schema, metadata).is_some() {
                found_illegal_fields.push("metadata");
            }
            if get_single_key!(schema, features).is_some() {
                found_illegal_fields.push("features");
            }
            if get_single_key!(schema, venv).is_some() {
                found_illegal_fields.push("venv");
            }
            if get_single_key!(schema, import).is_some() {
                found_illegal_fields.push("import");
            }

            if found_illegal_fields.is_empty() {
                return Ok(());
            }
            let err = qp_err!(
                "illegal field{} `{}` in the frontmatter at `{}`",
                found_illegal_fields.s_if_plural(),
                found_illegal_fields.join("`, `"),
                root.display()
            );
            qp_bail!(err.add_hint(
                "remove all the fields besides `dependencies`, `dev-dependencies` and `profiles`"
            ));
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
