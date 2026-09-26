//! Parsing of the {dev-,}dependencies fields in a manifest.
use tracing::debug;

use super::{ParseMode, ScopeGuard, source};
use crate::quackpack::core::lints::warnings::Warnings;
use crate::quackpack::core::valid_package_name::validate_package_name;
use crate::quackpack::core::{Conditions, Dependency, DependencyFeature, DependencyKind};
use crate::quackpack::schemas::OneEntryMap;
use crate::quackpack::schemas::manifest::{
    Dependencies as DependenciesSchema, Dependency as DependencySchema,
    DependencyCondition as ConditionSchema, DependencyFeature as FeatureSchema, DependencySource,
};
use crate::util::error::QuackResultContext;
use crate::{DuckContext, QuackResult, StrId};

/// Parse [`Dependency`]ies from the [`DependenciesSchema`], and append them into a vector.
///
/// It is a role of a caller to make sure that [`DependenciesSchema`] matches [`DependencyKind`].
#[tracing::instrument(skip_all)]
pub(crate) fn parse(
    schema: Option<&DependenciesSchema>,
    mode: ParseMode<'_>,
    kind: DependencyKind,
    dependencies: &mut Vec<Dependency>,
    warnings: &mut Warnings,
    ctx: &DuckContext,
    mut scope: ScopeGuard<'_>,
) -> QuackResult<()> {
    let Some(schema) = schema else {
        return Ok(());
    };
    for (name, dep_schema) in schema {
        let guard = scope.push(name.into());
        dependencies.push(parse_single_dependency(
            name.into(),
            dep_schema,
            mode,
            kind,
            warnings,
            ctx,
            guard,
        )?);
    }
    Ok(())
}

/// Parse single [`Dependency`] from its [`DependencySchema`].
#[tracing::instrument(skip_all, fields(name = %manifest_name))]
fn parse_single_dependency(
    manifest_name: StrId,
    schema: &DependencySchema,
    mode: ParseMode<'_>,
    kind: DependencyKind,
    warnings: &mut Warnings,
    ctx: &DuckContext,
    mut scope: ScopeGuard<'_>,
) -> QuackResult<Dependency> {
    debug!(?schema, "parsing a dependency");
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
    let source = source::parse(schema, mode, warnings, ctx, guard)?;

    let versions = schema
        .version
        .as_ref()
        .map(|ored| ored.0.clone())
        .unwrap_or_default();

    let features = parse_features(schema.features.as_ref());
    let pinned = schema.pinned.unwrap_or(false);
    let name = parse_name(schema).unwrap_or(manifest_name);

    let conditions = schema.conditions.as_ref().map(parse_conditions);
    Dependency::new(
        name, versions, source, features, pinned, conditions, alias, kind,
    )
    .with_context(|| scope.make_context_string())
}

/// Parse dependency's features
#[tracing::instrument(skip_all)]
fn parse_features(schema: Option<&Vec<FeatureSchema>>) -> Vec<DependencyFeature> {
    let Some(schema) = schema else {
        return vec![];
    };
    debug!(?schema);
    let mut result = vec![];
    for feature in schema {
        debug!(?feature, "parsing feature");
        match feature {
            FeatureSchema::Simple(name) => {
                result.push(DependencyFeature::new(name.into(), None));
            }
            FeatureSchema::Detailed(detailed_feature) => {
                let OneEntryMap {
                    key: ref name,
                    value: ref conditions,
                } = detailed_feature.0;
                result.push(DependencyFeature::new(
                    name.into(),
                    Some(parse_conditions(conditions)),
                ));
            }
        }
    }
    result
}

/// Parse [`Conditions`] from [`ConditionSchema`]
fn parse_conditions(schema: &ConditionSchema) -> Conditions {
    Conditions::new(
        schema
            .package_features
            .as_ref()
            .map(|vec| vec_string_to_vec_str_id(vec)),
    )
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
