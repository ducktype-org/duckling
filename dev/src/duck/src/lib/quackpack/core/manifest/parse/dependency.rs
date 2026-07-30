//! Parsing of the {dev-,}dependencies fields in a manifest.
use std::path::Path;

use tracing::{debug, trace};

use super::{ScopeGuard, source};
use crate::quackpack::core::valid_package_name::validate_package_name;
use crate::quackpack::core::{Conditions, Dependencies, Dependency, DependencyFeature};
use crate::quackpack::schemas::OneEntryMap;
use crate::quackpack::schemas::manifest::{
    Dependencies as DependenciesSchema, Dependency as DependencySchema,
    DependencyCondition as ConditionSchema, DependencyFeature as FeatureSchema, DependencySource,
};
use crate::util::error::QuackResultContext;
use crate::{DuckContext, QuackResult, StrId};

/// Parse [`Dependencies`] from the [`DependenciesSchema`].
#[tracing::instrument(skip_all)]
pub(crate) fn parse(
    schema: Option<&DependenciesSchema>,
    package_root: &Path,
    ctx: &DuckContext,
    mut scope: ScopeGuard<'_>,
) -> QuackResult<Dependencies> {
    let Some(schema) = schema else {
        return Dependencies::new(Vec::new());
    };
    let mut dependencies = Vec::new();
    for (name, dep_schema) in schema {
        let guard = scope.push(name.into());
        dependencies.push(parse_single_dependency(
            name.into(),
            dep_schema,
            package_root,
            ctx,
            guard,
        )?);
    }
    Dependencies::new(dependencies).with_context(|| scope.make_context_string())
}

/// Parse single [`Dependency`] from its [`DependencySchema`].
#[tracing::instrument(skip_all, fields(name = %manifest_name))]
fn parse_single_dependency(
    manifest_name: StrId,
    schema: &DependencySchema,
    package_root: &Path,
    ctx: &DuckContext,
    mut scope: ScopeGuard<'_>,
) -> QuackResult<Dependency> {
    trace!(?schema, "parsing a dependency");
    let name = parse_name(schema).unwrap_or(manifest_name);
    let alias = if name == manifest_name {
        None
    } else {
        Some(manifest_name)
    };
    validate_package_name(&name)
        .with_context(|| {
            if alias.is_none() {
                format!("dependency `{manifest_name}` has an invalid name")
            } else {
                format!("aliased dependency `{manifest_name}` points to a package `{name}` with an invalid name")
            }
        })
        .with_context(|| scope.make_context_string())?;
    if let Some(alias) = alias {
        validate_package_name(&alias)
            .with_context(|| format!("dependency `{name}` has an invalid alias name `{alias}`"))
            .with_context(|| scope.make_context_string())?;
    }
    let guard = scope.push("source".into());
    let source = source::parse(schema, package_root, ctx, guard)?;

    let versions = schema
        .version
        .as_ref()
        .map(|ored| ored.0.clone())
        .unwrap_or_default();

    let guard = scope.push("features".into());
    let features = parse_features(schema.features.as_ref(), guard)?;
    let pinned = schema.pinned.unwrap_or(false);
    let name = parse_name(schema).unwrap_or(manifest_name);

    let guard = scope.push("conditions".into());
    let conditions = schema
        .conditions
        .as_ref()
        .map(|conditions| parse_conditions(conditions, guard))
        .transpose()?;
    Dependency::new(name, versions, source, features, pinned, conditions, alias)
        .with_context(|| scope.make_context_string())
}

/// Parse dependency's features
#[tracing::instrument(skip_all)]
fn parse_features(
    schema: Option<&Vec<FeatureSchema>>,
    mut scope: ScopeGuard<'_>,
) -> QuackResult<Vec<DependencyFeature>> {
    let Some(schema) = schema else {
        return Ok(vec![]);
    };
    debug!(?schema);
    let mut result = vec![];
    for feature in schema {
        debug!("parsing {feature:?}");
        match feature {
            FeatureSchema::Simple(name) => {
                result.push(DependencyFeature::new(name.into(), None));
            }
            FeatureSchema::Detailed(detailed_feature) => {
                let OneEntryMap {
                    key: ref name,
                    value: ref conditions,
                } = detailed_feature.0;
                let mut name_guard = scope.push(name.into());
                let guard = name_guard.push("conditions".into());
                result.push(DependencyFeature::new(
                    name.into(),
                    Some(parse_conditions(conditions, guard)?),
                ));
            }
        }
    }
    Ok(result)
}

/// Parse [`Conditions`] from [`ConditionSchema`]
fn parse_conditions(schema: &ConditionSchema, scope: ScopeGuard<'_>) -> QuackResult<Conditions> {
    Conditions::new(
        schema
            .package_features
            .as_ref()
            .map(|vec| vec_string_to_vec_str_id(vec)),
    )
    .with_context(|| scope.make_context_string())
}

/// Helper for transforming slice of `&T: Into<StrId>` into `Vec<StrId>`
fn vec_string_to_vec_str_id<T>(input: &[T]) -> Vec<StrId>
where
    for<'a> &'a T: Into<StrId>,
{
    input.iter().map(<&T>::into).collect()
}

/// Parse the real name specified by the dependency.
fn parse_name(schema: &DependencySchema) -> Option<StrId> {
    match schema.source.as_ref() {
        Some(DependencySource::Detailed(detailed)) => detailed.name.as_ref().map(<&String>::into),
        _ => None,
    }
}
