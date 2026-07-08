use super::collect_packages;
use crate::quackpack::core::PackageLoader;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::early_graph::creating_graph::create_early_graph_from_bcx;
use crate::quackpack::core::compile::early_graph::tests::cycling::setup::*;
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit::Unit;
use crate::quackpack::core::compile::unit::graph::{UnitGraph, lower_early_graph};
use crate::quackpack::core::identity::{Identity, Origin};
use crate::quackpack::core::storage::paths::Storage;

#[test]
fn collects_packages() {
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
    };
    let graph = create_early_graph_from_bcx(&bcx).unwrap();
    let unit_graph = lower_early_graph(graph, &bcx);
    for unit in unit_graph.units_sorted_by_id() {
        let expected: &[&str] = match unit.root_package().package().name().as_str() {
            "root" => &["bar", "baz", "foo", "root"],
            "foo" => &["baz", "foo"],
            "bar" => &["bar", "baz"],
            "baz" => &["baz"],
            _ => unreachable!(),
        };
        assert_packages_names(unit, &unit_graph, expected);
    }
}

#[test]
fn collects_packages_cycle() {
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
    };
    let graph = create_early_graph_from_bcx(&bcx).unwrap();
    let unit_graph = lower_early_graph(graph, &bcx);
    for unit in unit_graph.units_sorted_by_id() {
        let expected: &[&str] = match unit.root_package().package().name().as_str() {
            "root" | "cycle" => &["bar", "cycle", "foo", "root"],
            "foo" => &["foo"],
            "bar" => &["bar"],
            _ => unreachable!(),
        };
        assert_packages_names(unit, &unit_graph, expected);
    }
}

fn assert_packages_names(unit: &Unit, graph: &UnitGraph, expected: &[&str]) {
    let mut names = collect_packages(unit, graph)
        .into_iter()
        .map(|package| package.import_name.as_str())
        .collect::<Vec<_>>();
    names.sort();
    assert_eq!(names, expected)
}
