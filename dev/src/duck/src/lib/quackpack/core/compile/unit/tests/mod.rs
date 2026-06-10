use std::collections::HashMap;

use super::graph::lower_early_graph;
use crate::quackpack::core::PackageLoader;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::early_graph::creating_graph::create_early_graph_from_bcx;
use crate::quackpack::core::compile::early_graph::tests::cycling::setup::*;
use crate::quackpack::core::compile::executor::debug_executor::DebugExecutor;
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit::ArtifactsType;
use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::identity::{Identity, Origin};
use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::util::to_url::ToUrl;

// **IMPORTANT**
// Some notes on the tests' structure:
// * we use [0u64; 0] to create an empty slice of u64; otherwise, there's also a serde_json's Value,
//   which can be compared against u64, and rustc complains about not-infering the type.
// * since ID's are random, firstly we collect them by name.

// **NOTE**
// To de-duplicate some code, we reuse setup from early_graph/ tests.

#[test]
fn lowers_early_graph() {
    let (ctx, root) = setup_mock_storage();
    let identity_for = |name: &str| {
        let path = root.path().join(name);
        let origin = Origin::for_local(&path).unwrap();
        Identity::new(name.into(), origin)
    };

    let fetcher_identity_for = |name: &str| {
        let origin = Origin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
        Identity::new(name.into(), origin)
    };
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let root_origin = Origin::for_local(&root.path().join("root")).unwrap();
    let root_identity = Identity::new("root".into(), root_origin);
    let bcx = BuildContext {
        executor: Box::new(DebugExecutor),
        pcx: &package,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["full".into()],
        profile,
        script_path: None,
    };
    let graph = create_early_graph_from_bcx(&bcx).unwrap();
    let unit_graph = lower_early_graph(graph, &bcx);

    let name_to_id = {
        let mut map = HashMap::new();
        for (id, unit) in unit_graph.units.iter() {
            let name = unit.root_package().package().manifest().name().as_str();
            map.insert(name, *id);
        }
        map
    };
    let root_id = *name_to_id.get("root").unwrap();
    let foo_id = *name_to_id.get("foo").unwrap();
    let bar_id = *name_to_id.get("bar").unwrap();
    let baz_id = *name_to_id.get("baz").unwrap();
    assert!(!name_to_id.contains_key("cycle"));
    assert_eq!(name_to_id.len(), 4);

    let root = unit_graph.unit_for(root_id);
    assert_eq!(root, unit_graph.root_unit());
    assert_eq!(root.artifacts_type(), ArtifactsType::Binary);
    assert_eq!(root.unit_id(), root_id);
    let mut deps = root.deps_by_unit_id().to_vec();
    deps.sort();
    let mut real_deps = [foo_id, bar_id];
    real_deps.sort();
    assert_eq!(real_deps[..], deps[..]);
    assert_eq!(root.identity(), identity_for("root"));

    let foo = unit_graph.unit_for(foo_id);
    assert_eq!(foo.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(foo.unit_id(), foo_id);
    assert_eq!(foo.deps_by_unit_id(), [baz_id]);
    assert_eq!(foo.identity(), fetcher_identity_for("foo"));

    let bar = unit_graph.unit_for(bar_id);
    assert_eq!(bar.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(bar.unit_id(), bar_id);
    assert_eq!(bar.deps_by_unit_id(), [baz_id]);
    assert_eq!(bar.identity(), fetcher_identity_for("bar"));

    let baz = unit_graph.unit_for(baz_id);
    assert_eq!(baz.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(baz.unit_id(), baz_id);
    assert_eq!(baz.deps_by_unit_id(), [0u64; 0]);
    assert_eq!(baz.identity(), fetcher_identity_for("baz"));
}

#[test]
fn lowers_early_graph_with_cycle() {
    let (ctx, root) = setup_mock_storage();
    let identity_for = |name: &str| {
        let path = root.path().join(name);
        let origin = Origin::for_local(&path).unwrap();
        Identity::new(name.into(), origin)
    };

    let fetcher_identity_for = |name: &str| {
        let origin = Origin::for_registry(Fetcher::DEFAULT_REGISTRY_URL.to_url().unwrap());
        Identity::new(name.into(), origin)
    };
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let root_origin = Origin::for_local(&root.path().join("root")).unwrap();
    let root_identity = Identity::new("root".into(), root_origin);
    let bcx = BuildContext {
        executor: Box::new(DebugExecutor),
        pcx: &package,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["cycle".into()],
        profile,
        script_path: None,
    };
    let graph = create_early_graph_from_bcx(&bcx).unwrap();
    let unit_graph = lower_early_graph(graph, &bcx);

    let name_to_id = {
        let mut map = HashMap::new();
        for (id, unit) in unit_graph.units.iter() {
            let name = unit.root_package().package().manifest().name().as_str();
            map.insert(name, *id);
        }
        map
    };
    let root_id = *name_to_id.get("root").unwrap();
    let foo_id = *name_to_id.get("foo").unwrap();
    let bar_id = *name_to_id.get("bar").unwrap();
    let cycle_id = *name_to_id.get("cycle").unwrap();
    assert!(!name_to_id.contains_key("baz"));
    assert_eq!(name_to_id.len(), 4);

    let root = unit_graph.unit_for(root_id);
    assert_eq!(root, unit_graph.root_unit());
    assert_eq!(root.artifacts_type(), ArtifactsType::Binary);
    assert_eq!(root.unit_id(), root_id);
    assert_eq!(root.identity(), identity_for("root"));
    let mut deps = root.deps_by_unit_id().to_vec();
    deps.sort();
    let mut real_deps = [foo_id, bar_id, cycle_id];
    real_deps.sort();
    assert_eq!(real_deps[..], deps[..]);

    let foo = unit_graph.unit_for(foo_id);
    assert_eq!(foo.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(foo.unit_id(), foo_id);
    assert_eq!(foo.deps_by_unit_id(), [0u64; 0]);
    assert_eq!(foo.identity(), fetcher_identity_for("foo"));

    let bar = unit_graph.unit_for(bar_id);
    assert_eq!(bar.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(bar.unit_id(), bar_id);
    assert_eq!(bar.deps_by_unit_id(), [0u64; 0]);
    assert_eq!(bar.identity(), fetcher_identity_for("bar"));

    let cycle = unit_graph.unit_for(cycle_id);
    assert_eq!(cycle.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(cycle.unit_id(), cycle_id);
    assert_eq!(cycle.deps_by_unit_id(), [root_id]);
    assert_eq!(cycle.identity(), identity_for("cycle"));
}
