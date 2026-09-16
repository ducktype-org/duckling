use std::collections::{HashMap, HashSet};
use std::path::PathBuf;
use std::time::Duration;

use futures::executor::block_on;
use httpmock::prelude::*;
use tempfile::{TempDir, tempdir};

use crate::DuckContext;
use crate::quackpack::core::fetcher::{Fetcher, types};
use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
use crate::quackpack::core::solver::gathering::gatherer::Gatherer;
use crate::quackpack::core::solver::git_access::GitAccess;
use crate::quackpack::core::solver::solver_mode::SolverMode;
use crate::quackpack::core::{FeatureName, PackageId, Source, Version, parse_manifest};
use crate::quackpack::schemas::OneEntryMap;
use crate::quackpack::schemas::registry::{self, DependencyCondition, DependencyFeature};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::to_url::ToUrl;
use crate::util::path_ops_ext::PathOpsExt;
use crate::util::test_utils::setup_test;

struct MockGitAccess();
impl GitAccess for MockGitAccess {
    fn git_path(&self, _url: InternedUrl, _commit: &str) -> PathBuf {
        unimplemented!()
    }

    fn is_stored(&self, _url: InternedUrl, _commit: &str) -> bool {
        unimplemented!()
    }

    fn store(
        &self,
        _url: InternedUrl,
        _commit: &str,
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
        // SAFETY: Setup is single threaded, and `Env` in `DuckContext`, copies all envs.
        unsafe {
            std::env::set_var("DUCK_HOME", dir.path());
        }
        let ctx = DuckContext::default();
        // SAFETY: Setup is single threaded, and `Env` in `DuckContext`, copies all envs.
        unsafe {
            std::env::remove_var("DUCK_HOME");
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
        kind: registry::DependencyKind::Normal,
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
            authors: vec!["Carly Shillingford".into()],
            license: "MIT".into(),
            name: "foo".into(),
            description: "".into(),
            links: None,
        },
        dependencies: vec![foo_bar_dep],
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let foo2 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(2, 0, 0),
            authors: vec!["Carly Shillingford".into()],
            license: "MIT".into(),
            name: "foo".into(),
            description: "".into(),
            links: None,
        },
        dependencies: [].into(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let bar3 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(3, 0, 0),
            authors: vec!["Carly Shillingford".into()],
            license: "MIT".into(),
            name: "bar".into(),
            description: "".into(),
            links: None,
        },
        dependencies: [].into(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let bar411 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(4, 1, 1),
            authors: vec!["Carly Shillingford".into()],
            license: "MIT".into(),
            name: "bar".into(),
            description: "".into(),
            links: None,
        },
        dependencies: [].into(),
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
        kind: registry::DependencyKind::Normal,
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
            authors: vec!["Carly Shillingford".into()],
            license: "MIT".into(),
            name: "xd".into(),
            description: "".into(),
            links: None,
        },
        dependencies: [].into(),
        features: [("dx".into(), vec![])].into(),
        profiles: HashMap::new(),
    };

    let dx2 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(2, 0, 0),
            authors: vec!["Carly Shillingford".into()],
            license: "MIT".into(),
            name: "dx".into(),
            description: "".into(),
            links: None,
        },
        dependencies: vec![dx_xd_dep],
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
        kind: registry::DependencyKind::Normal,
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
        kind: registry::DependencyKind::Normal,
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
            authors: vec!["Carly Shillingford".into()],
            license: "MIT".into(),
            name: "a".into(),
            description: "".into(),
            links: None,
        },
        dependencies: vec![a_c_dep],
        features: [("f".into(), vec![])].into(),
        profiles: HashMap::new(),
    };

    let a2 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(2, 0, 0),
            authors: vec!["Carly Shillingford".into()],
            license: "MIT".into(),
            name: "a".into(),
            description: "".into(),
            links: None,
        },
        dependencies: [].into(),
        features: [].into(),
        profiles: HashMap::new(),
    };

    let b1 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 0, 0),
            authors: vec!["Carly Shillingford".into()],
            license: "MIT".into(),
            name: "b".into(),
            description: "".into(),
            links: None,
        },
        dependencies: vec![b_a_dep],
        features: [].into(),
        profiles: HashMap::new(),
    };

    let c1 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 0, 0),
            authors: vec!["Carly Shillingford".into()],
            license: "MIT".into(),
            name: "c".into(),
            description: "".into(),
            links: None,
        },
        dependencies: [].into(),
        features: [].into(),
        profiles: HashMap::new(),
    };

    // Assets for cycle test.
    let u_v_dep = registry::Dependency {
        name: "v".into(),
        version: vec![Version::new(1, 0, 0)],
        source: registry::DependencySource {
            inner: registry::SourceInner::Registry {
                registry_url: server.base_url(),
            },
        },
        kind: registry::DependencyKind::Normal,
        features: vec![DependencyFeature::Simple("u".into())],
        pinned: true,
        conditions: registry::DependencyCondition {
            package_features: None,
        },
        alias: None,
    };

    let v_u_dep = registry::Dependency {
        name: "u".into(),
        version: vec![Version::new(1, 0, 0)],
        source: registry::DependencySource {
            inner: registry::SourceInner::Registry {
                registry_url: server.base_url(),
            },
        },
        kind: registry::DependencyKind::Normal,
        features: vec![DependencyFeature::Simple("v".into())],
        pinned: true,
        conditions: registry::DependencyCondition {
            package_features: None,
        },
        alias: None,
    };

    let u = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 0, 0),
            authors: vec!["Carly Shillingford".into()],
            license: "MIT".into(),
            name: "u".into(),
            description: "".into(),
            links: None,
        },
        dependencies: vec![u_v_dep],
        features: [("v".into(), vec![])].into(),
        profiles: HashMap::new(),
    };

    let v = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 0, 0),
            authors: vec!["Carly Shillingford".into()],
            license: "MIT".into(),
            name: "v".into(),
            description: "".into(),
            links: None,
        },
        dependencies: vec![v_u_dep],
        features: [("u".into(), vec![])].into(),
        profiles: HashMap::new(),
    };

    // Assets for features_expansion test.
    let n_m_dep = registry::Dependency {
        name: "m".into(),
        version: vec![Version::new(1, 0, 0)],
        source: registry::DependencySource {
            inner: registry::SourceInner::Registry {
                registry_url: server.base_url(),
            },
        },
        kind: registry::DependencyKind::Normal,
        features: vec![],
        pinned: true,
        conditions: registry::DependencyCondition {
            package_features: Some(vec!["expanded".into()]),
        },
        alias: None,
    };

    let n = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 0, 0),
            authors: vec!["Carly Shillingford".into()],
            license: "MIT".into(),
            name: "n".into(),
            description: "".into(),
            links: None,
        },
        dependencies: vec![n_m_dep],
        features: [
            ("expandable".into(), vec!["expanded".into()]),
            ("expanded".into(), vec![]),
        ]
        .into(),
        profiles: HashMap::new(),
    };

    let m = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 0, 0),
            authors: vec!["Carly Shillingford".into()],
            license: "MIT".into(),
            name: "m".into(),
            description: "".into(),
            links: None,
        },
        dependencies: vec![],
        features: [].into(),
        profiles: HashMap::new(),
    };

    // Assets for not_pinned_registry test.
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

    // Assets for cycle test.
    server.mock(|when, then| {
        when.method(GET).path("/packages/u/1.0.0");
        then.status(200).json_body_obj(&u);
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/v/1.0.0");
        then.status(200).json_body_obj(&v);
    });

    // Assets for features_expansion test.
    server.mock(|when, then| {
        when.method(GET).path("/packages/n/1.0.0");
        then.status(200).json_body_obj(&n);
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/m/1.0.0");
        then.status(200).json_body_obj(&m);
    });

    server
}

#[test]
/// Not pinned registry dependencies test.
/// Synopsis:
/// * root depends on foo 1 or 2,
/// * foo 1 depends on bar 3 or 4
fn not_pinned_registry() {
    let (ctx, _root) = setup_duck_ctx();
    let server = create_mock_server();
    let url: InternedUrl = server.base_url().to_url().unwrap().into();
    let fetcher = Fetcher::new(&ctx).unwrap();
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
    let root_name = "root".into();
    let root_version = Version::new(0, 1, 0);
    let root_manifest = parse_manifest(&root_path, &ctx).unwrap().0.into_manifest();
    let git_access = MockGitAccess();
    let gatherer = Gatherer::new(&fetcher, &git_access);
    let gathered_info = block_on(gatherer.explore(
        root_path.clone(),
        root_manifest,
        HashSet::new(),
        SolverMode::default(),
    ))
    .unwrap();
    let identity_root = FullIdentity::new(root_name, FullOrigin::for_local(&root_path).unwrap());
    let identity_foo = FullIdentity::new("foo".into(), FullOrigin::for_registry(url));
    let identity_bar = FullIdentity::new("bar".into(), FullOrigin::for_registry(url));
    assert_eq!(
        gathered_info.versions_for_identity,
        HashMap::from([
            (identity_root, HashSet::from([root_version])),
            (
                identity_foo,
                HashSet::from([Version::new(1, 0, 0), Version::new(2, 0, 0)])
            ),
            (
                identity_bar,
                HashSet::from([Version::new(3, 0, 0), Version::new(4, 1, 1)])
            ),
        ])
    );
    let packages = HashSet::from([
        PackageId::new(identity_root, root_version),
        PackageId::new(identity_foo, Version::new(1, 0, 0)),
        PackageId::new(identity_foo, Version::new(2, 0, 0)),
        PackageId::new(identity_bar, Version::new(3, 0, 0)),
        PackageId::new(identity_bar, Version::new(4, 1, 1)),
    ]);
    assert_eq!(
        packages,
        gathered_info
            .packages_data
            .keys()
            .copied()
            .collect::<HashSet<PackageId>>()
    );
    for features in gathered_info
        .packages_data
        .values()
        .map(|data| &data.requested_features)
    {
        assert!(features.is_empty());
    }
    assert_eq!(
        gathered_info.source_to_origin_resolver,
        HashMap::from([
            (
                Source::for_local(&root_path).unwrap(),
                identity_root.origin()
            ),
            (Source::for_registry(url), identity_foo.origin())
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

    let url: InternedUrl = server.base_url().to_url().unwrap().into();
    let fetcher = Fetcher::new(&ctx).unwrap();
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
    let root_name = "root".into();
    let root_version = Version::new(0, 1, 0);
    let root_manifest = parse_manifest(&root_path, &ctx).unwrap().0.into_manifest();
    let git_access = MockGitAccess();
    let gatherer = Gatherer::new(&fetcher, &git_access);
    let gathered_info = block_on(gatherer.explore(
        root_path.clone(),
        root_manifest,
        HashSet::new(),
        SolverMode::default(),
    ))
    .unwrap();
    let identity_root = FullIdentity::new(root_name, FullOrigin::for_local(&root_path).unwrap());
    let identity_xd = FullIdentity::new("xd".into(), FullOrigin::for_registry(url));
    let identity_dx = FullIdentity::new("dx".into(), FullOrigin::for_registry(url));
    assert_eq!(
        gathered_info.versions_for_identity,
        HashMap::from([
            (identity_root, HashSet::from([root_version])),
            (identity_xd, HashSet::from([Version::new(1, 0, 0)])),
            (identity_dx, HashSet::from([Version::new(2, 0, 0)])),
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

    let url: InternedUrl = server.base_url().to_url().unwrap().into();
    let fetcher = Fetcher::new(&ctx).unwrap();
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
    let root_name = "root".into();
    let root_version = Version::new(0, 1, 0);
    let root_manifest = parse_manifest(&root_path, &ctx).unwrap().0.into_manifest();
    let git_access = MockGitAccess();
    let gatherer = Gatherer::new(&fetcher, &git_access);
    let gathered_info = block_on(gatherer.explore(
        root_path.clone(),
        root_manifest,
        ["my_feature".into()].into(),
        SolverMode::default(),
    ))
    .unwrap();
    let identity_root = FullIdentity::new(root_name, FullOrigin::for_local(&root_path).unwrap());
    let identity_xd = FullIdentity::new("xd".into(), FullOrigin::for_registry(url));
    let identity_dx = FullIdentity::new("dx".into(), FullOrigin::for_registry(url));
    assert_eq!(
        gathered_info
            .packages_data
            .into_iter()
            .map(|(pkg, data)| (pkg, data.requested_features))
            .collect::<HashMap<PackageId, HashSet<FeatureName>>>(),
        HashMap::from([
            (
                PackageId::new(identity_root, root_version),
                ["my_feature".into()].into()
            ),
            (
                PackageId::new(identity_xd, Version::new(1, 0, 0)),
                ["dx".into()].into()
            ),
            (
                PackageId::new(identity_dx, Version::new(2, 0, 0)),
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

    let url: InternedUrl = server.base_url().to_url().unwrap().into();
    let fetcher = Fetcher::new(&ctx).unwrap();
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
    let root_name = "root".into();
    let root_version = Version::new(0, 1, 0);
    let root_manifest = parse_manifest(&root_path, &ctx).unwrap().0.into_manifest();
    let git_access = MockGitAccess();
    let gatherer = Gatherer::new(&fetcher, &git_access);
    let gathered_info = block_on(gatherer.explore(
        root_path.clone(),
        root_manifest,
        [].into(),
        SolverMode::default(),
    ))
    .unwrap();
    let identity_root = FullIdentity::new(root_name, FullOrigin::for_local(&root_path).unwrap());
    let identity_a = FullIdentity::new("a".into(), FullOrigin::for_registry(url));
    let identity_b = FullIdentity::new("b".into(), FullOrigin::for_registry(url));
    let identity_c = FullIdentity::new("c".into(), FullOrigin::for_registry(url));
    assert_eq!(
        gathered_info.versions_for_identity,
        HashMap::from([
            (identity_root, [root_version].into()),
            (
                identity_a,
                [Version::new(1, 0, 0), Version::new(2, 0, 0)].into()
            ),
            (identity_b, [Version::new(1, 0, 0)].into()),
            (identity_c, [Version::new(1, 0, 0)].into()),
        ])
    );
    assert_eq!(
        gathered_info
            .packages_data
            .into_iter()
            .map(|(pkg, data)| (pkg, data.requested_features))
            .collect::<HashMap<PackageId, HashSet<FeatureName>>>(),
        HashMap::from([
            (PackageId::new(identity_root, root_version), [].into()),
            (
                PackageId::new(identity_a, Version::new(1, 0, 0)),
                ["f".into()].into()
            ),
            (PackageId::new(identity_a, Version::new(2, 0, 0)), [].into()),
            (PackageId::new(identity_b, Version::new(1, 0, 0)), [].into()),
            (PackageId::new(identity_c, Version::new(1, 0, 0)), [].into()),
        ])
    )
}

/// Cycle of dependencies test.
/// Synopsis:
/// * root depends on *u* in version precisely 1
/// * *u* depends on *v* in version precisely 1, with feature *u*
/// * *v* depends on *u* in version precisely 1, with feature *v*
#[test]
fn cycle() {
    let (ctx, _root) = setup_duck_ctx();
    let server = create_mock_server();

    let url: InternedUrl = server.base_url().to_url().unwrap().into();
    let fetcher = Fetcher::new(&ctx).unwrap();
    let (_dir, root_path) = prepare_manifest(&format!(
        r#"
metadata:
  name: root
  version: '0.1.0'

dependencies:
  u:
    source:
      registry-url: {}
    version: '1'
    pinned: true
"#,
        &url
    ));
    let root_name = "root".into();
    let root_version = Version::new(0, 1, 0);
    let root_manifest = parse_manifest(&root_path, &ctx).unwrap().0.into_manifest();
    let git_access = MockGitAccess();
    let gatherer = Gatherer::new(&fetcher, &git_access);
    let gathered_info = block_on(gatherer.explore(
        root_path.clone(),
        root_manifest,
        [].into(),
        SolverMode::default(),
    ))
    .unwrap();
    let identity_root = FullIdentity::new(root_name, FullOrigin::for_local(&root_path).unwrap());
    let identity_u = FullIdentity::new("u".into(), FullOrigin::for_registry(url));
    let identity_v = FullIdentity::new("v".into(), FullOrigin::for_registry(url));
    assert_eq!(
        gathered_info.versions_for_identity,
        HashMap::from([
            (identity_root, [root_version].into()),
            (identity_u, [Version::new(1, 0, 0)].into()),
            (identity_v, [Version::new(1, 0, 0)].into()),
        ])
    );
    assert_eq!(
        gathered_info
            .packages_data
            .into_iter()
            .map(|(pkg, data)| (pkg, data.requested_features))
            .collect::<HashMap<PackageId, HashSet<FeatureName>>>(),
        HashMap::from([
            (PackageId::new(identity_root, root_version), [].into()),
            (
                PackageId::new(identity_u, Version::new(1, 0, 0)),
                ["v".into()].into()
            ),
            (
                PackageId::new(identity_v, Version::new(1, 0, 0)),
                ["u".into()].into()
            ),
        ])
    )
}

/// Features expansion test.
/// Synopsis:
/// * root depends on *n* in version precisely 1, with feature *expandable*, expanding to *expanded*
/// * *n* depends on *m* in version precisely 1, conditioned on *expanded*
#[test]
fn features_expansion() {
    let (ctx, _root) = setup_duck_ctx();
    let server = create_mock_server();

    let url: InternedUrl = server.base_url().to_url().unwrap().into();
    let fetcher = Fetcher::new(&ctx).unwrap();
    let (_dir, root_path) = prepare_manifest(&format!(
        r#"
metadata:
  name: root
  version: '0.1.0'

dependencies:
  n:
    source:
      registry-url: {}
    version: '1'
    pinned: true
    features: [expandable]
"#,
        &url
    ));
    let root_name = "root".into();
    let root_version = Version::new(0, 1, 0);
    let root_manifest = parse_manifest(&root_path, &ctx).unwrap().0.into_manifest();
    let git_access = MockGitAccess();
    let gatherer = Gatherer::new(&fetcher, &git_access);
    let gathered_info = block_on(gatherer.explore(
        root_path.clone(),
        root_manifest,
        [].into(),
        SolverMode::default(),
    ))
    .unwrap();
    let identity_root = FullIdentity::new(root_name, FullOrigin::for_local(&root_path).unwrap());
    let identity_n = FullIdentity::new("n".into(), FullOrigin::for_registry(url));
    let identity_m = FullIdentity::new("m".into(), FullOrigin::for_registry(url));
    assert_eq!(
        gathered_info.versions_for_identity,
        HashMap::from([
            (identity_root, [root_version].into()),
            (identity_n, [Version::new(1, 0, 0)].into()),
            (identity_m, [Version::new(1, 0, 0)].into()),
        ])
    );
    assert_eq!(
        gathered_info
            .packages_data
            .into_iter()
            .map(|(pkg, data)| (pkg, data.requested_features))
            .collect::<HashMap<PackageId, HashSet<FeatureName>>>(),
        HashMap::from([
            (PackageId::new(identity_root, root_version), [].into()),
            (
                PackageId::new(identity_n, Version::new(1, 0, 0)),
                ["expandable".into(), "expanded".into()].into()
            ),
            (PackageId::new(identity_m, Version::new(1, 0, 0)), [].into()),
        ])
    )
}
