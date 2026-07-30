use super::graph::lower_early_graph;
use crate::QuackResult;
use crate::quackpack::core::PackageLoader;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::early_graph::creating_graph::create_early_graph_from_bcx;
use crate::quackpack::core::compile::early_graph::tests::cycling::setup::*;
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit::unit_visitor::UnitVisitor;
use crate::quackpack::core::compile::unit::{ArtifactsType, Unit};
use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::identity::{Identity, Origin};
use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::util::to_url::ToUrl;

// **IMPORTANT**
// Some notes on the tests' structure:
// * we use [0u64; 0] to create an empty slice of u64; otherwise, there's also a serde_json's Value,
//   which can be compared against u64, and rustc complains about not-infering the type.

// **NOTE**
// To de-duplicate some code, we reuse setup from early_graph/ tests.

#[derive(Default)]
struct IdOrder(Vec<u64>);

impl UnitVisitor for IdOrder {
    fn visit(&mut self, unit: &Unit) -> QuackResult<()> {
        self.0.push(unit.unit_id());
        Ok(())
    }
}

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
        pcx: &package,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["full".into()],
        profile,
        shared: false,
        jobs: 1,
    };
    let graph = create_early_graph_from_bcx(&bcx).unwrap();
    let unit_graph = lower_early_graph(graph, &bcx);

    let root_id = 0;
    let foo_id = 2;
    let bar_id = 1;
    let baz_id = 3;
    assert_eq!(unit_graph.units_sorted_by_id().len(), 4);

    let root = unit_graph.unit_for(root_id);
    assert_eq!(root, unit_graph.root_unit());
    assert_eq!(root.artifacts_type(), ArtifactsType::Binary);
    assert_eq!(root.unit_id(), root_id);
    assert_eq!(root.deps_sorted_by_unit_id(), [bar_id, foo_id]);
    assert_eq!(root.identity(), identity_for("root"));

    let foo = unit_graph.unit_for(foo_id);
    assert_eq!(foo.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(foo.unit_id(), foo_id);
    assert_eq!(foo.deps_sorted_by_unit_id(), [baz_id]);
    assert_eq!(foo.identity(), fetcher_identity_for("foo"));

    let bar = unit_graph.unit_for(bar_id);
    assert_eq!(bar.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(bar.unit_id(), bar_id);
    assert_eq!(bar.deps_sorted_by_unit_id(), [baz_id]);
    assert_eq!(bar.identity(), fetcher_identity_for("bar"));

    let baz = unit_graph.unit_for(baz_id);
    assert_eq!(baz.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(baz.unit_id(), baz_id);
    assert_eq!(baz.deps_sorted_by_unit_id(), [0u64; 0]);
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
        pcx: &package,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["cycle".into()],
        profile,
        shared: false,
        jobs: 1,
    };
    let graph = create_early_graph_from_bcx(&bcx).unwrap();
    let unit_graph = lower_early_graph(graph, &bcx);

    let root_id = 0;
    let foo_id = 3;
    let bar_id = 1;
    let cycle_id = 2;
    assert_eq!(unit_graph.units_sorted_by_id().len(), 4);

    let root = unit_graph.unit_for(root_id);
    assert_eq!(root, unit_graph.root_unit());
    assert_eq!(root.artifacts_type(), ArtifactsType::Binary);
    assert_eq!(root.unit_id(), root_id);
    assert_eq!(root.identity(), identity_for("root"));
    assert_eq!(root.deps_sorted_by_unit_id(), [bar_id, cycle_id, foo_id]);

    let foo = unit_graph.unit_for(foo_id);
    assert_eq!(foo.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(foo.unit_id(), foo_id);
    assert_eq!(foo.deps_sorted_by_unit_id(), [0u64; 0]);
    assert_eq!(foo.identity(), fetcher_identity_for("foo"));

    let bar = unit_graph.unit_for(bar_id);
    assert_eq!(bar.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(bar.unit_id(), bar_id);
    assert_eq!(bar.deps_sorted_by_unit_id(), [0u64; 0]);
    assert_eq!(bar.identity(), fetcher_identity_for("bar"));

    let cycle = unit_graph.unit_for(cycle_id);
    assert_eq!(cycle.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(cycle.unit_id(), cycle_id);
    assert_eq!(cycle.deps_sorted_by_unit_id(), [root_id]);
    assert_eq!(cycle.identity(), identity_for("cycle"));
}

#[test]
fn basic_visitor_order_cycle() {
    let (ctx, root) = setup_mock_storage();
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let root_origin = Origin::for_local(&root.path().join("root")).unwrap();
    let root_identity = Identity::new("root".into(), root_origin);
    let bcx = BuildContext {
        pcx: &package,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["cycle".into()],
        profile,
        shared: false,
        jobs: 1,
    };
    let graph = create_early_graph_from_bcx(&bcx).unwrap();
    let unit_graph = lower_early_graph(graph, &bcx);
    let root_id = 0;
    let foo_id = 3;
    let bar_id = 1;
    let cycle_id = 2;
    {
        let mut visitor = IdOrder::default();

        unit_graph
            .root_unit()
            .accept(&mut visitor, &unit_graph)
            .unwrap();

        assert_eq!(visitor.0, [root_id, bar_id, cycle_id, foo_id]);
    }
    {
        let mut visitor = IdOrder::default();

        unit_graph
            .unit_for(cycle_id)
            .accept(&mut visitor, &unit_graph)
            .unwrap();

        assert_eq!(visitor.0, [cycle_id, root_id, bar_id, foo_id]);
    }
    {
        let mut visitor = IdOrder::default();
        unit_graph
            .unit_for(foo_id)
            .accept(&mut visitor, &unit_graph)
            .unwrap();

        assert_eq!(visitor.0, [foo_id]);
    }
}

#[test]
fn basic_visitor_order() {
    let (ctx, root) = setup_mock_storage();
    let package = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), package.package().manifest().profiles()).unwrap();
    let root_origin = Origin::for_local(&root.path().join("root")).unwrap();
    let root_identity = Identity::new("root".into(), root_origin);
    let bcx = BuildContext {
        pcx: &package,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["full".into()],
        profile,
        shared: false,
        jobs: 1,
    };
    let graph = create_early_graph_from_bcx(&bcx).unwrap();
    let unit_graph = lower_early_graph(graph, &bcx);

    let root_id = 0;
    let foo_id = 2;
    let bar_id = 1;
    let baz_id = 3;

    {
        let mut visitor = IdOrder::default();

        unit_graph
            .root_unit()
            .accept(&mut visitor, &unit_graph)
            .unwrap();

        assert_eq!(visitor.0, [root_id, bar_id, foo_id, baz_id]);
    }
    {
        let mut visitor = IdOrder::default();

        unit_graph
            .unit_for(foo_id)
            .accept(&mut visitor, &unit_graph)
            .unwrap();

        assert_eq!(visitor.0, [foo_id, baz_id]);
    }
    {
        let mut visitor = IdOrder::default();

        unit_graph
            .unit_for(bar_id)
            .accept(&mut visitor, &unit_graph)
            .unwrap();

        assert_eq!(visitor.0, [bar_id, baz_id]);
    }

    {
        let mut visitor = IdOrder::default();

        unit_graph
            .unit_for(baz_id)
            .accept(&mut visitor, &unit_graph)
            .unwrap();

        assert_eq!(visitor.0, [baz_id]);
    }
}
