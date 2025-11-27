use std::{
    collections::{BTreeMap, HashMap},
    path::Path,
};

use anyhow::{Context, bail};
use tracing::debug;

use super::dependency;

use crate::{
    QpCtx, QuackResult, StrId,
    quackpack::{
        core::{CompilerSpecificOptions, Features, Manifest, Profiles, RootDescription},
        schemas::manifest::{CompilerOptions, Manifest as ManifestSchema},
    },
    static_str_id,
};

use super::Scope;

pub(crate) fn parse(schema: &ManifestSchema, root: &Path, ctx: QpCtx<'_>) -> QuackResult<Manifest> {
    let Some(ref metadata) = schema.metadata else {
        bail!("missing the obligatory section `metadata`")
    };
    let Some(version) = metadata.version else {
        bail!("missing the obligatory key `metadata.version`")
    };
    let Some(ref name) = metadata.name else {
        bail!("missing the obligatory key `metadata.name`")
    };
    debug!("package name is `{name}`, version is `{version}`");
    let root_description = RootDescription::new(name.into(), version);
    let mut scope = Scope::new();
    scope.push(static_str_id!("dependencies"));
    let dependencies = dependency::parse(schema.dependencies.as_ref(), root, ctx, &mut scope)?;
    scope.pop();

    scope.push(static_str_id!("dev_dependencies"));
    let dev_deps = dependency::parse(schema.dev_dependencies.as_ref(), root, ctx, &mut scope)?;
    scope.pop();

    scope.push(static_str_id!("features"));
    let features = parse_features(schema.features.as_ref())
        .with_context(|| format!("when parsing the field `{}`", scope.format()))?;
    scope.pop();
    let profiles = Profiles::new(parse_compiler_flags(schema.profiles.as_ref()));
    let authors = metadata
        .authors
        .as_ref()
        .map(|vec| vec.iter().map(|x| x.into()).collect())
        .unwrap_or_default();
    Ok(Manifest::new(
        root_description,
        features,
        authors,
        metadata.license.as_ref().map(|x| x.into()),
        metadata.description.as_ref().map(|x| x.into()),
        dependencies,
        dev_deps,
        profiles,
    ))
}

fn parse_features(features: Option<&BTreeMap<String, Vec<String>>>) -> QuackResult<Features> {
    let Some(features) = features else {
        return Features::new(HashMap::new());
    };
    let as_hash_map = features
        .iter()
        .map(|(k, v)| (k.into(), v.iter().map(|x| x.into()).collect()))
        .collect();
    Features::new(as_hash_map)
}

fn parse_compiler_flags(
    input: Option<&BTreeMap<String, CompilerOptions>>,
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
                    .map(|vec| vec.iter().map(|x| x.into()).collect())
                    .unwrap_or_default(),
            );
            (k.into(), opts)
        })
        .collect()
}
