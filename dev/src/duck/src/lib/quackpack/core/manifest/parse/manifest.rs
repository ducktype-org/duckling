use std::{collections::HashMap, path::Path};

use tracing::debug;

use super::dependency;

use crate::{
    QpCtx, QuackResult, QuackResultContext, StrId, qp_bail,
    quackpack::{
        core::{
            CompilerSpecificOptions, Features, Manifest, PackageMetadata, Profiles, RootDescription,
        },
        schemas::manifest::{CompilerOptions, Manifest as ManifestSchema},
    },
};

use super::Scope;

/// Parse [`Manifest`] from given [`ManifestSchema`].
pub(crate) fn parse(
    schema: &ManifestSchema,
    root: &Path,
    ctx: &QpCtx<'_>,
) -> QuackResult<Manifest> {
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
    let root_description = RootDescription::new(name.into(), version);
    let mut scope = Scope::new();
    scope.push("dependencies".into());
    let dependencies = dependency::parse(schema.dependencies.as_ref(), root, ctx, &mut scope)?;
    scope.pop();

    scope.push("dev_dependencies".into());
    let dev_deps = dependency::parse(schema.dev_dependencies.as_ref(), root, ctx, &mut scope)?;
    scope.pop();

    scope.push("features".into());
    let features = parse_features(schema.features.as_ref())
        .with_context(|| format!("when parsing the field `{}`", scope.format()))?;
    scope.pop();

    let profiles = Profiles::new(parse_compiler_flags(schema.profiles.as_ref()));

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
        root_description,
        features,
        package_metadata,
        dependencies,
        dev_deps,
        profiles,
    ))
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

/// Parse [`CompilerSpecificOptions`] from the given compiler flags mapping.
fn parse_compiler_flags(
    input: Option<&HashMap<String, CompilerOptions>>,
) -> HashMap<StrId, CompilerSpecificOptions> {
    let Some(input) = input else {
        return HashMap::new();
    };
    input
        .iter()
        .map(|(k, v)| {
            let opts = CompilerSpecificOptions::new(
                v.compiler_flags
                    .as_ref()
                    .map(|vec| vec.iter().map(<&String>::into).collect())
                    .unwrap_or_default(),
            );
            (k.into(), opts)
        })
        .collect()
}
