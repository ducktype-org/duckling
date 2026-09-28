use std::ops::ControlFlow;

use super::graph::lower_early_graph;
use super::unit_visitor::UnitVisitor;
use super::{ArtifactsType, Unit, UnitId};
use crate::quackpack::core::PackageLoader;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::early_graph::creating_graph::create_early_graph_from_bcx;
use crate::quackpack::core::compile::early_graph::tests::cycling::setup::*;
use crate::quackpack::core::compile::early_graph::tests::{
    mock_local_identity, mock_local_pkg, mock_registry_identity, mock_registry_pkg,
};
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::fetcher::Fetcher;
use crate::quackpack::core::storage::load_deps::load_packages_in_freeze as load_packages;
use crate::quackpack::core::storage::paths::Storage;

// **IMPORTANT**
// Some notes on the tests' structure:
// * we use [0u64; 0] to create an empty slice of u64; otherwise, there's also a serde_json's Value,
//   which can be compared against u64, and rustc complains about not-infering the type.

// **NOTE**
// To de-duplicate some code, we reuse setup from early_graph/ tests.

#[derive(Default)]
struct IdOrder(Vec<UnitId>);

impl UnitVisitor for IdOrder {
    type Break = ();
    fn visit(&mut self, unit: &Unit) -> ControlFlow<Self::Break> {
        self.0.push(unit.unit_id());
        ControlFlow::Continue(())
    }
}

#[test]
fn lowers_early_graph() {
    let (ctx, root) = setup_mock_storage();
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
            mock_local_pkg(root.path(), "cycle"),
            mock_registry_pkg("foo"),
            mock_registry_pkg("bar"),
            mock_registry_pkg("baz"),
        ],
    )
    .unwrap();
    let root_pkg = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), root_pkg.package().manifest().profiles()).unwrap();
    let root_identity = mock_local_identity(root.path(), "root").into();
    let bcx = BuildContext {
        pcx: &root_pkg,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["full".into()],
        profile,
        shared: false,
        jobs: 1,
    };
    let graph = create_early_graph_from_bcx(&bcx, packages.pkgs).unwrap();
    let unit_graph = lower_early_graph(graph, &bcx);

    let root_id = 0;
    let foo_id = 2;
    let bar_id = 1;
    let baz_id = 3;
    assert_eq!(unit_graph.units_sorted_by_id().len(), 4);

    let deps_for = |unit: &Unit| unit_graph.deps_for(unit.unit_id());

    let root_unit = unit_graph.unit_for(root_id);
    assert_eq!(root_unit, unit_graph.root_unit());
    assert_eq!(root_unit.artifacts_type(), ArtifactsType::Binary);
    assert_eq!(root_unit.unit_id(), root_id);
    assert_eq!(deps_for(root_unit), [bar_id, foo_id]);
    assert_eq!(
        root_unit.identity(),
        mock_local_identity(root.path(), "root")
    );

    let foo = unit_graph.unit_for(foo_id);
    assert_eq!(foo.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(foo.unit_id(), foo_id);
    assert_eq!(deps_for(foo), [baz_id]);
    assert_eq!(foo.identity(), mock_registry_identity("foo"));

    let bar = unit_graph.unit_for(bar_id);
    assert_eq!(bar.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(bar.unit_id(), bar_id);
    assert_eq!(deps_for(bar), [baz_id]);
    assert_eq!(bar.identity(), mock_registry_identity("bar"));

    let baz = unit_graph.unit_for(baz_id);
    assert_eq!(baz.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(baz.unit_id(), baz_id);
    assert_eq!(deps_for(baz), [0u64; 0]);
    assert_eq!(baz.identity(), mock_registry_identity("baz"));
}

#[test]
fn lowers_early_graph_for_dvm() {
    let (ctx, root) = setup_mock_storage();
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
            mock_local_pkg(root.path(), "cycle"),
            mock_registry_pkg("foo"),
            mock_registry_pkg("bar"),
            mock_registry_pkg("baz"),
        ],
    )
    .unwrap();
    let root_pkg = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let mut profile =
        Profile::construct_profile("dev".into(), root_pkg.package().manifest().profiles()).unwrap();
    profile.dvm_bytecode = true;
    let root_identity = mock_local_identity(root.path(), "root").into();
    let bcx = BuildContext {
        pcx: &root_pkg,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["full".into()],
        profile,
        shared: false,
        jobs: 1,
    };
    let graph = create_early_graph_from_bcx(&bcx, packages.pkgs).unwrap();
    let unit_graph = lower_early_graph(graph, &bcx);

    let root_id = 0;
    let foo_id = 2;
    let bar_id = 1;
    let baz_id = 3;
    assert_eq!(unit_graph.units_sorted_by_id().len(), 4);

    let deps_for = |unit: &Unit| unit_graph.deps_for(unit.unit_id());

    let root_unit = unit_graph.unit_for(root_id);
    assert_eq!(root_unit, unit_graph.root_unit());
    assert_eq!(root_unit.artifacts_type(), ArtifactsType::Dvm);
    assert_eq!(root_unit.unit_id(), root_id);
    assert_eq!(deps_for(root_unit), [bar_id, foo_id]);
    assert_eq!(
        root_unit.identity(),
        mock_local_identity(root.path(), "root")
    );

    let foo = unit_graph.unit_for(foo_id);
    assert_eq!(foo.artifacts_type(), ArtifactsType::DvmDependency);
    assert_eq!(foo.unit_id(), foo_id);
    assert_eq!(deps_for(foo), [baz_id]);
    assert_eq!(foo.identity(), mock_registry_identity("foo"));

    let bar = unit_graph.unit_for(bar_id);
    assert_eq!(bar.artifacts_type(), ArtifactsType::DvmDependency);
    assert_eq!(bar.unit_id(), bar_id);
    assert_eq!(deps_for(bar), [baz_id]);
    assert_eq!(bar.identity(), mock_registry_identity("bar"));

    let baz = unit_graph.unit_for(baz_id);
    assert_eq!(baz.artifacts_type(), ArtifactsType::DvmDependency);
    assert_eq!(baz.unit_id(), baz_id);
    assert_eq!(deps_for(baz), [0u64; 0]);
    assert_eq!(baz.identity(), mock_registry_identity("baz"));
}

#[test]
fn lowers_early_graph_with_cycle() {
    let (ctx, root) = setup_mock_storage();
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
            mock_local_pkg(root.path(), "cycle"),
            mock_registry_pkg("foo"),
            mock_registry_pkg("bar"),
            mock_registry_pkg("baz"),
        ],
    )
    .unwrap();
    let root_pkg = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), root_pkg.package().manifest().profiles()).unwrap();
    let root_identity = mock_local_identity(root.path(), "root").into();
    let bcx = BuildContext {
        pcx: &root_pkg,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["cycle".into()],
        profile,
        shared: false,
        jobs: 1,
    };
    let graph = create_early_graph_from_bcx(&bcx, packages.pkgs).unwrap();
    let unit_graph = lower_early_graph(graph, &bcx);

    let root_id = 0;
    let foo_id = 3;
    let bar_id = 1;
    let cycle_id = 2;
    assert_eq!(unit_graph.units_sorted_by_id().len(), 4);

    let root_unit = unit_graph.unit_for(root_id);
    assert_eq!(root_unit, unit_graph.root_unit());
    assert_eq!(root_unit.artifacts_type(), ArtifactsType::Binary);
    assert_eq!(root_unit.unit_id(), root_id);
    assert_eq!(
        root_unit.identity(),
        mock_local_identity(root.path(), "root")
    );

    let deps_for = |unit: &Unit| unit_graph.deps_for(unit.unit_id());
    assert_eq!(deps_for(root_unit), [bar_id, cycle_id, foo_id]);

    let foo = unit_graph.unit_for(foo_id);
    assert_eq!(foo.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(foo.unit_id(), foo_id);
    assert_eq!(deps_for(foo), [0u64; 0]);
    assert_eq!(foo.identity(), mock_registry_identity("foo"));

    let bar = unit_graph.unit_for(bar_id);
    assert_eq!(bar.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(bar.unit_id(), bar_id);
    assert_eq!(deps_for(bar), [0u64; 0]);
    assert_eq!(bar.identity(), mock_registry_identity("bar"));

    let cycle = unit_graph.unit_for(cycle_id);
    assert_eq!(cycle.artifacts_type(), ArtifactsType::IsADependencyArtifact);
    assert_eq!(cycle.unit_id(), cycle_id);
    assert_eq!(deps_for(cycle), [root_id]);
    assert_eq!(cycle.identity(), mock_local_identity(root.path(), "cycle"));
}

#[test]
fn basic_visitor_order_cycle() {
    let (ctx, root) = setup_mock_storage();
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
            mock_local_pkg(root.path(), "cycle"),
            mock_registry_pkg("foo"),
            mock_registry_pkg("bar"),
            mock_registry_pkg("baz"),
        ],
    )
    .unwrap();
    let root_pkg = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), root_pkg.package().manifest().profiles()).unwrap();
    let root_identity = mock_local_identity(root.path(), "root").into();
    let bcx = BuildContext {
        pcx: &root_pkg,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["cycle".into()],
        profile,
        shared: false,
        jobs: 1,
    };
    let graph = create_early_graph_from_bcx(&bcx, packages.pkgs).unwrap();
    let unit_graph = lower_early_graph(graph, &bcx);
    let root_id = 0;
    let foo_id = 3;
    let bar_id = 1;
    let cycle_id = 2;
    {
        let mut visitor = IdOrder::default();

        unit_graph.root_unit().accept(&mut visitor, &unit_graph);

        assert_eq!(visitor.0, [root_id, bar_id, cycle_id, foo_id]);
    }
    {
        let mut visitor = IdOrder::default();

        unit_graph
            .unit_for(cycle_id)
            .accept(&mut visitor, &unit_graph);

        assert_eq!(visitor.0, [cycle_id, root_id, bar_id, foo_id]);
    }
    {
        let mut visitor = IdOrder::default();
        unit_graph
            .unit_for(foo_id)
            .accept(&mut visitor, &unit_graph);

        assert_eq!(visitor.0, [foo_id]);
    }
}

#[test]
fn basic_visitor_order() {
    let (ctx, root) = setup_mock_storage();
    let storage = Storage::new(root.path().join("storage"));
    let fetcher = Fetcher::new(&ctx).unwrap();

    let packages = load_packages(
        &storage,
        &fetcher,
        vec![
            mock_local_pkg(root.path(), "root"),
            mock_local_pkg(root.path(), "cycle"),
            mock_registry_pkg("foo"),
            mock_registry_pkg("bar"),
            mock_registry_pkg("baz"),
        ],
    )
    .unwrap();
    let root_pkg = PackageLoader::find_at_exact_directory(&root.path().join("root"), &ctx).unwrap();
    let profile =
        Profile::construct_profile("dev".into(), root_pkg.package().manifest().profiles()).unwrap();
    let root_identity = mock_local_identity(root.path(), "root").into();
    let bcx = BuildContext {
        pcx: &root_pkg,
        root_identity,
        freeze: freeze(root.path()),
        storage: Storage::new(ctx.default_storage_root().into_not_locked_path()),
        used_features: vec!["full".into()],
        profile,
        shared: false,
        jobs: 1,
    };
    let graph = create_early_graph_from_bcx(&bcx, packages.pkgs).unwrap();
    let unit_graph = lower_early_graph(graph, &bcx);

    let root_id = 0;
    let foo_id = 2;
    let bar_id = 1;
    let baz_id = 3;

    {
        let mut visitor = IdOrder::default();

        unit_graph.root_unit().accept(&mut visitor, &unit_graph);

        assert_eq!(visitor.0, [root_id, bar_id, foo_id, baz_id]);
    }
    {
        let mut visitor = IdOrder::default();

        unit_graph
            .unit_for(foo_id)
            .accept(&mut visitor, &unit_graph);

        assert_eq!(visitor.0, [foo_id, baz_id]);
    }
    {
        let mut visitor = IdOrder::default();

        unit_graph
            .unit_for(bar_id)
            .accept(&mut visitor, &unit_graph);

        assert_eq!(visitor.0, [bar_id, baz_id]);
    }

    {
        let mut visitor = IdOrder::default();

        unit_graph
            .unit_for(baz_id)
            .accept(&mut visitor, &unit_graph);

        assert_eq!(visitor.0, [baz_id]);
    }
}
