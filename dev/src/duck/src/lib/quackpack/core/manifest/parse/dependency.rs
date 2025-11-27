use std::{collections::HashMap, path::Path};

use super::source;
use anyhow::Context;
use tracing::Level;
use tracing::debug;
use tracing::span;

use super::Scope;

use crate::StrId;
use crate::quackpack::core::Conditions;
use crate::quackpack::core::Dependency;
use crate::quackpack::core::DependencyDescription;
use crate::quackpack::core::DependencyFeature;
use crate::quackpack::schemas::manifest::Dependencies as DependenciesSchema;
use crate::quackpack::schemas::manifest::Dependency as DependencySchema;
use crate::quackpack::schemas::manifest::DependencyCondition as ConditionSchema;
use crate::quackpack::schemas::manifest::DependencyFeature as FeatureSchema;
use crate::quackpack::schemas::manifest::DependencySource;

use crate::static_str_id;
use crate::{QpCtx, QuackResult, quackpack::core::Dependencies};

pub(crate) fn parse(
    schema: Option<&DependenciesSchema>,
    package_root: &Path,
    ctx: QpCtx<'_>,
    scope: &mut Scope,
) -> QuackResult<Dependencies> {
    let Some(schema) = schema else {
        return Ok(Dependencies::new(HashMap::new()));
    };
    let mut dependencies = HashMap::new();
    for (name, dep_schema) in schema {
        let name = name.into();
        let span = span!(Level::DEBUG, "dependency", name = %name);
        let _guard = span.enter();
        scope.push(name);
        dependencies.insert(
            name,
            parse_single_dependency(name, dep_schema, package_root, ctx, scope)?,
        );
        scope.pop();
    }
    Ok(Dependencies::new(dependencies))
}

fn parse_single_dependency(
    manifest_name: StrId,
    schema: &DependencySchema,
    package_root: &Path,
    ctx: QpCtx<'_>,
    scope: &mut Scope,
) -> QuackResult<Dependency> {
    debug!("parsing a dependency");
    scope.push(static_str_id!("source"));
    let source = source::parse(schema, package_root, ctx, scope)?;
    scope.pop();

    let versions = schema
        .version
        .as_ref()
        .map(|ored| ored.0.clone())
        .unwrap_or_default();
    let desc = DependencyDescription::new(manifest_name, versions, source)
        .with_context(|| format!("when parsing the field `{}`", scope.format()))?;

    scope.push(static_str_id!("features"));
    let features = parse_features(schema.features.as_ref(), scope)?;
    scope.pop();
    let pinned = schema.pinned.unwrap_or(false);
    let real_name = parse_real_name(schema).unwrap_or(manifest_name);

    scope.push(static_str_id!("conditions"));
    let conditions = schema
        .conditions
        .as_ref()
        .map(|conditions| parse_conditions(conditions, scope))
        .transpose()?;
    scope.pop();
    Dependency::new(desc, features, pinned, conditions, real_name)
        .with_context(|| format!("when parsing the field `{}`", scope.format()))
}

fn parse_features(
    schema: Option<&Vec<FeatureSchema>>,
    scope: &mut Scope,
) -> QuackResult<Vec<DependencyFeature>> {
    let Some(schema) = schema else {
        return Ok(vec![]);
    };
    let mut result = vec![];
    for feature in schema {
        match feature {
            FeatureSchema::Simple(name) => {
                result.push(DependencyFeature::new(name.into(), None));
            }
            FeatureSchema::Detailed(detailed_feature) => {
                for (name, conditions) in detailed_feature.0.iter() {
                    scope.push(name.into());
                    scope.push(static_str_id!("conditions"));
                    result.push(DependencyFeature::new(
                        name.into(),
                        Some(parse_conditions(conditions, scope)?),
                    ));
                    scope.pop();
                    scope.pop();
                }
            }
        }
    }
    Ok(result)
}

fn parse_conditions(schema: &ConditionSchema, scope: &Scope) -> QuackResult<Conditions> {
    Conditions::new(
        schema
            .package_features
            .as_ref()
            .map(|vec| vec_string_to_vec_str_id(vec)),
    )
    .with_context(|| format!("when parsing the field `{}`", scope.format()))
}

fn vec_string_to_vec_str_id<T>(input: &[T]) -> Vec<StrId>
where
    for<'a> &'a T: Into<StrId>,
{
    input.iter().map(|string| string.into()).collect()
}

fn parse_real_name(schema: &DependencySchema) -> Option<StrId> {
    match schema.source.as_ref() {
        Some(DependencySource::Detailed(detailed)) => {
            detailed.name.as_ref().map(|name| name.into())
        }
        _ => None,
    }
}
