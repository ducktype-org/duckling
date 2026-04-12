use std::{collections::HashMap, path::PathBuf, time::Duration};

use httpmock::prelude::*;
use tempfile::{TempDir, tempdir};

use crate::{
    DuckContext,
    quackpack::{
        core::{Version, fetcher::types, git_access::GitAccess},
        schemas::{
            OneEntryMap,
            registry::{self, DependencyCondition, DependencyFeature},
        },
    },
    util::{path_ops_ext::PathOpsExt, test_utils::setup_test},
};

use std::collections::HashSet;

use url::Url;

use crate::quackpack::core::{
    fetcher::Fetcher,
    gathering::gatherer::Gatherer,
    parse_manifest,
    solver_mode::SolverMode,
    types_common::{
        ExpandedLocation, ExpandedPackage, InternedExpandedLocation, InternedLocation, Location,
    },
};

struct MockGitAccess();
impl GitAccess for MockGitAccess {
    fn git_path(&self, _url: url::Url, _commit: crate::StrId) -> PathBuf {
        unimplemented!()
    }

    fn is_stored(&self, _url: url::Url, _commit: crate::StrId) -> bool {
        unimplemented!()
    }

    fn store(
        &mut self,
        _url: url::Url,
        _commit: crate::StrId,
        _source_path: &std::path::Path,
    ) -> crate::QuackResult<()> {
        unimplemented!()
    }
}

fn setup_duck_ctx() -> (DuckContext, TempDir) {
    let setup = || {
        // We set cache directory to a temporary directory, so we can use `Fetcher` without
        // worrying about leaving traces of tests in FS.
        let dir = tempdir().unwrap();
        // SAFETY: Setup is single threaded, and `Env` in `DuckCtx`, copies all envs.
        unsafe {
            std::env::set_var("DUCK_CACHE_DIR", dir.path());
        }
        let ctx = DuckContext::default();
        // SAFETY: Setup is single threaded, and `Env` in `DuckCtx`, copies all envs.
        unsafe {
            std::env::remove_var("DUCK_CACHE_DIR");
        }
        (ctx, dir)
    };
    setup_test(setup)
}

fn prepare_manifest(contents: &str) -> (TempDir, PathBuf) {
    let dir = tempdir().unwrap();
    let manifest = dir.path().join("x");
    manifest.touch().unwrap();
    manifest.write(contents).unwrap();
    (dir, manifest)
}

fn create_mock_server() -> MockServer {
    let server = MockServer::start();

    // Assets for not_pinned_registry test.
    let foo_bar_dep = registry::Dependency {
        name: "bar".into(),
        version: vec![Version::new(3, 0, 0), Version::new(4, 0, 0)],
        source: registry::DependencySource {
            inner: registry::SourceInner::Registry {
                registry_url: server.base_url(),
            },
        },
        features: vec![],
        pinned: false,
        conditions: registry::DependencyCondition {
            package_features: None,
        },
        alias: None,
    };

    let foo1 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "foo".into(),
            description: "".into(),
        },
        dependencies: vec![foo_bar_dep],
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let foo2 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(2, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "foo".into(),
            description: "".into(),
        },
        dependencies: [].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let bar3 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(3, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "bar".into(),
            description: "".into(),
        },
        dependencies: [].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let bar411 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(4, 1, 1),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "bar".into(),
            description: "".into(),
        },
        dependencies: [].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    // Assets for pinned_registry and features tests.
    let dx_xd_dep = registry::Dependency {
        name: "xd".into(),
        version: vec![Version::new(1, 0, 0)],
        source: registry::DependencySource {
            inner: registry::SourceInner::Registry {
                registry_url: server.base_url(),
            },
        },
        features: vec![DependencyFeature::Detailed(OneEntryMap {
            key: "dx".into(),
            value: DependencyCondition {
                package_features: Some(vec!["root".into()]),
            },
        })],
        pinned: true,
        conditions: registry::DependencyCondition {
            package_features: None,
        },
        alias: None,
    };

    let xd1 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "xd".into(),
            description: "".into(),
        },
        dependencies: [].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: [("dx".into(), vec![])].into(),
        profiles: HashMap::new(),
    };

    let dx2 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(2, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "dx".into(),
            description: "".into(),
        },
        dependencies: vec![dx_xd_dep],
        dev_dependencies: registry::Dependencies::new(),
        features: [("root".into(), vec![])].into(),
        profiles: HashMap::new(),
    };

    // Assets for pinned_request_while_pending_not_pinned test.
    let b_a_dep = registry::Dependency {
        name: "a".into(),
        version: vec![Version::new(1, 0, 0)],
        source: registry::DependencySource {
            inner: registry::SourceInner::Registry {
                registry_url: server.base_url(),
            },
        },
        features: vec![DependencyFeature::Simple("f".into())],
        pinned: true,
        conditions: registry::DependencyCondition {
            package_features: None,
        },
        alias: None,
    };

    let a_c_dep = registry::Dependency {
        name: "c".into(),
        version: vec![Version::new(1, 0, 0)],
        source: registry::DependencySource {
            inner: registry::SourceInner::Registry {
                registry_url: server.base_url(),
            },
        },
        features: vec![],
        pinned: false,
        conditions: registry::DependencyCondition {
            package_features: Some(vec!["f".into()]),
        },
        alias: None,
    };

    let a1 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "a".into(),
            description: "".into(),
        },
        dependencies: vec![a_c_dep],
        dev_dependencies: registry::Dependencies::new(),
        features: [("f".into(), vec![])].into(),
        profiles: HashMap::new(),
    };

    let a2 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(2, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "a".into(),
            description: "".into(),
        },
        dependencies: [].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: [].into(),
        profiles: HashMap::new(),
    };

    let b1 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "b".into(),
            description: "".into(),
        },
        dependencies: vec![b_a_dep],
        dev_dependencies: registry::Dependencies::new(),
        features: [].into(),
        profiles: HashMap::new(),
    };

    let c1 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "c".into(),
            description: "".into(),
        },
        dependencies: [].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: [].into(),
        profiles: HashMap::new(),
    };

    server.mock(|when, then| {
        when.method(GET).path("/packages/foo/1.0.0");
        then.status(200).json_body_obj(&foo1);
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/foo/2.0.0");
        then.status(200).json_body_obj(&foo2);
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/foo");
        then.status(200).json_body_obj(&types::MultiMetadata {
            packages_metadata: vec![foo1, foo2],
        });
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/bar/3.0.0");
        then.status(200).json_body_obj(&bar3);
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/bar/4.1.1");
        then.status(200).json_body_obj(&bar411);
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/bar");
        then.status(200).json_body_obj(&types::MultiMetadata {
            packages_metadata: vec![bar3, bar411],
        });
    });

    // Assets for pinned_registry and features tests.
    server.mock(|when, then| {
        when.method(GET).path("/packages/xd/1.0.0");
        then.status(200).json_body_obj(&xd1);
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/dx/2.0.0");
        then.status(200)
            .json_body_obj(&dx2)
            .delay(Duration::from_secs(1));
    });

    // Assets for pinned_request_while_pending_not_pinned test.
    server.mock(|when, then| {
        when.method(GET).path("/packages/a/1.0.0");
        then.status(200).json_body_obj(&a1);
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/a");
        then.status(200)
            .json_body_obj(&types::MultiMetadata {
                packages_metadata: vec![a1, a2],
            })
            .delay(Duration::from_millis(200));
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/b");
        then.status(200)
            .json_body_obj(&types::MultiMetadata {
                packages_metadata: vec![b1],
            })
            .delay(Duration::from_millis(100));
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/c");
        then.status(200).json_body_obj(&types::MultiMetadata {
            packages_metadata: vec![c1],
        });
    });

    server
}

#[test]
/// Not pinned registry dependencies test.
/// Synopsis:
/// * root depends on foo 1.0.0 or 2.0.0,
/// * foo 1.0.0 depends on bar 3.0.0 or 4.0.0
fn not_pinned_registry() {
    let (ctx, _root) = setup_duck_ctx();
    let server = create_mock_server();
    let url: Url = server.base_url().parse().unwrap();
    let mut fetcher = Fetcher::new(&ctx).unwrap();
    let (_dir, root_path) = prepare_manifest(&format!(
        r#"
metadata:
  name: root
  version: '0.1.0'

dependencies:
  foo:
    source:
      registry-url: {}
    version: 1 or 2
"#,
        &url
    ));
    let root_manifest = parse_manifest(&root_path, &ctx).unwrap();
    let mut git_access = MockGitAccess();
    let mut gatherer = Gatherer::new(&mut fetcher, &mut git_access);
    let gathered_info = gatherer
        .explore(
            root_path.clone(),
            root_manifest.manifest().clone(),
            HashSet::new(),
            SolverMode::default(),
        )
        .unwrap();
    let loc_root = InternedExpandedLocation::new(ExpandedLocation::Local {
        absolute_path: root_path.clone(),
    });
    let loc_foo = InternedExpandedLocation::new(ExpandedLocation::Registry {
        url: url.clone(),
        real_name: "foo".into(),
    });
    let loc_bar = InternedExpandedLocation::new(ExpandedLocation::Registry {
        url: url.clone(),
        real_name: "bar".into(),
    });
    assert!(
        gathered_info.versions_for_location
            == HashMap::from([
                (loc_root, HashSet::from([None])),
                (
                    loc_foo,
                    HashSet::from([Some(Version::new(1, 0, 0)), Some(Version::new(2, 0, 0))])
                ),
                (
                    loc_bar,
                    HashSet::from([Some(Version::new(3, 0, 0)), Some(Version::new(4, 1, 1))])
                ),
            ])
    );
    let packages = HashSet::from([
        ExpandedPackage {
            location: loc_root,
            version: None,
        },
        ExpandedPackage {
            location: loc_foo,
            version: Some(Version::new(1, 0, 0)),
        },
        ExpandedPackage {
            location: loc_foo,
            version: Some(Version::new(2, 0, 0)),
        },
        ExpandedPackage {
            location: loc_bar,
            version: Some(Version::new(3, 0, 0)),
        },
        ExpandedPackage {
            location: loc_bar,
            version: Some(Version::new(4, 1, 1)),
        },
    ]);
    assert!(
        gathered_info
            .gathered_manifests
            .keys()
            .copied()
            .collect::<HashSet<ExpandedPackage>>()
            == packages
    );
    assert!(
        gathered_info
            .possible_features
            .keys()
            .copied()
            .collect::<HashSet<ExpandedPackage>>()
            == packages
    );
    for (_, features) in gathered_info.possible_features {
        assert!(features.is_empty());
    }
    assert!(
        gathered_info.location_resolver
            == HashMap::from([
                (
                    InternedLocation::new(Location::Local { path: root_path }),
                    loc_root
                ),
                (
                    InternedLocation::new(Location::Registry {
                        url: url.clone(),
                        real_name: "foo".into()
                    }),
                    loc_foo
                ),
                (
                    InternedLocation::new(Location::Registry {
                        url: url.clone(),
                        real_name: "bar".into()
                    }),
                    loc_bar
                ),
            ])
    )
}

#[test]
/// Pinned registry dependencies test.
/// Synopsis:
/// * root depends on xd exactly 1.0.0,
/// * root depends on dx exactly 2.0.0,
/// * dx depends exactly on xd 1.0.0
fn pinned_registry() {
    let (ctx, _root) = setup_duck_ctx();
    let server = create_mock_server();

    let url: Url = server.base_url().parse().unwrap();
    let mut fetcher = Fetcher::new(&ctx).unwrap();
    let (_dir, root_path) = prepare_manifest(&format!(
        r#"
metadata:
  name: root
  version: '0.1.0'

dependencies:
  xd:
    source:
      registry-url: {}
    version: '1'
    pinned: true
  dx:
    source:
      registry-url: {}
    version: '2'
    pinned: true
"#,
        &url, &url,
    ));
    let root_manifest = parse_manifest(&root_path, &ctx).unwrap();
    let mut git_access = MockGitAccess();
    let mut gatherer = Gatherer::new(&mut fetcher, &mut git_access);
    let gathered_info = gatherer
        .explore(
            root_path.clone(),
            root_manifest.manifest().clone(),
            HashSet::new(),
            SolverMode::default(),
        )
        .unwrap();
    let loc_root = InternedExpandedLocation::new(ExpandedLocation::Local {
        absolute_path: root_path.clone(),
    });
    let loc_xd = InternedExpandedLocation::new(ExpandedLocation::Registry {
        url: url.clone(),
        real_name: "xd".into(),
    });
    let loc_dx = InternedExpandedLocation::new(ExpandedLocation::Registry {
        url: url.clone(),
        real_name: "dx".into(),
    });
    assert!(
        gathered_info.versions_for_location
            == HashMap::from([
                (loc_root, HashSet::from([None])),
                (loc_xd, HashSet::from([Some(Version::new(1, 0, 0))])),
                (loc_dx, HashSet::from([Some(Version::new(2, 0, 0))])),
            ])
    );
}

#[test]
/// Features propagation test.
/// Same scenario as in [`pinned_registry`], but additionally:
/// * root has feature *my_feature*,
/// * this feature forces dx to have feature *root*
/// * this feature forces xd to have feature *dx*
fn features() {
    let (ctx, _root) = setup_duck_ctx();
    let server = create_mock_server();

    let url: Url = server.base_url().parse().unwrap();
    let mut fetcher = Fetcher::new(&ctx).unwrap();
    let (_dir, root_path) = prepare_manifest(&format!(
        r#"
metadata:
  name: root
  version: '0.1.0'

dependencies:
  xd:
    source:
      registry-url: {}
    version: '1'
    pinned: true
  dx:
    source:
      registry-url: {}
    features:
    - root:
        package-features: [my_feature] 
    version: '2'
    pinned: true

features:
  my_feature: []
"#,
        &url, &url,
    ));
    let root_manifest = parse_manifest(&root_path, &ctx).unwrap();
    let mut git_access = MockGitAccess();
    let mut gatherer = Gatherer::new(&mut fetcher, &mut git_access);
    let gathered_info = gatherer
        .explore(
            root_path.clone(),
            root_manifest.manifest().clone(),
            ["my_feature".into()].into(),
            SolverMode::default(),
        )
        .unwrap();
    let loc_root = InternedExpandedLocation::new(ExpandedLocation::Local {
        absolute_path: root_path.clone(),
    });
    let loc_xd = InternedExpandedLocation::new(ExpandedLocation::Registry {
        url: url.clone(),
        real_name: "xd".into(),
    });
    let loc_dx = InternedExpandedLocation::new(ExpandedLocation::Registry {
        url: url.clone(),
        real_name: "dx".into(),
    });
    assert!(
        gathered_info.possible_features
            == HashMap::from([
                (
                    ExpandedPackage {
                        location: loc_root,
                        version: None,
                    },
                    ["my_feature".into()].into()
                ),
                (
                    ExpandedPackage {
                        location: loc_xd,
                        version: Some(Version::new(1, 0, 0)),
                    },
                    ["dx".into()].into()
                ),
                (
                    ExpandedPackage {
                        location: loc_dx,
                        version: Some(Version::new(2, 0, 0)),
                    },
                    ["root".into()].into()
                ),
            ])
    );
}
#[test]
/// Pinned request while pending not pinned request test.
/// Synopsis:
/// * root depends on *a* in version either 1 or 2
/// * root depends on *b* in version 1
/// * *b* depends on *a* in version precisely 1, with feature *f*
/// * *a* in version 1 with feature *f* depends on *c*
/// * fetcher responds to *a* not pinned request with a delay of 200ms
/// * fetcher responds to *b* not pinned request with a delay of 100ms
///
/// What happens:
/// 1. Not pinned fetches of *a* and *b* are requested.
/// 2. Fetch of *b* succeeds first, but only after fetch of *a* is requested, because of the delay.
/// 3. Pinned request of *a* in version 1 is chained to the not pinned request.
/// 4. Not pinned request succeeds.
/// 5. A request for *c* is made.
fn pinned_request_while_pending_not_pinned() {
    let (ctx, _root) = setup_duck_ctx();
    let server = create_mock_server();

    let url: Url = server.base_url().parse().unwrap();
    let mut fetcher = Fetcher::new(&ctx).unwrap();
    let (_dir, root_path) = prepare_manifest(&format!(
        r#"
metadata:
  name: root
  version: '0.1.0'

dependencies:
  a:
    source:
      registry-url: {}
    version: 1 or 2
  b:
    source:
      registry-url: {}
    version: '1'
"#,
        &url, &url,
    ));
    let root_manifest = parse_manifest(&root_path, &ctx).unwrap();
    let mut git_access = MockGitAccess();
    let mut gatherer = Gatherer::new(&mut fetcher, &mut git_access);
    let gathered_info = gatherer
        .explore(
            root_path.clone(),
            root_manifest.manifest().clone(),
            [].into(),
            SolverMode::default(),
        )
        .unwrap();
    let loc_root = InternedExpandedLocation::new(ExpandedLocation::Local {
        absolute_path: root_path.clone(),
    });
    let loc_a = InternedExpandedLocation::new(ExpandedLocation::Registry {
        url: url.clone(),
        real_name: "a".into(),
    });
    let loc_b = InternedExpandedLocation::new(ExpandedLocation::Registry {
        url: url.clone(),
        real_name: "b".into(),
    });
    let loc_c = InternedExpandedLocation::new(ExpandedLocation::Registry {
        url: url.clone(),
        real_name: "c".into(),
    });
    assert!(
        gathered_info.versions_for_location
            == HashMap::from([
                (loc_root, [None].into()),
                (
                    loc_a,
                    [Some(Version::new(1, 0, 0)), Some(Version::new(2, 0, 0))].into()
                ),
                (loc_b, [Some(Version::new(1, 0, 0))].into()),
                (loc_c, [Some(Version::new(1, 0, 0))].into()),
            ])
    );
    assert!(
        gathered_info.possible_features
            == HashMap::from([
                (
                    ExpandedPackage {
                        location: loc_root,
                        version: None,
                    },
                    [].into()
                ),
                (
                    ExpandedPackage {
                        location: loc_a,
                        version: Some(1.into()),
                    },
                    ["f".into()].into()
                ),
                (
                    ExpandedPackage {
                        location: loc_a,
                        version: Some(2.into()),
                    },
                    [].into()
                ),
                (
                    ExpandedPackage {
                        location: loc_b,
                        version: Some(1.into()),
                    },
                    [].into()
                ),
                (
                    ExpandedPackage {
                        location: loc_c,
                        version: Some(1.into()),
                    },
                    [].into()
                ),
            ])
    )
}
