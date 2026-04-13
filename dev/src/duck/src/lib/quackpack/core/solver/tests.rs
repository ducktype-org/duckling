use std::collections::HashMap;
use std::path::PathBuf;

use httpmock::prelude::*;
use tempfile::{TempDir, tempdir};
use url::Url;

use crate::DuckContext;
use crate::quackpack::core::fetcher::{Fetcher, types};
use crate::quackpack::core::solver::git_access::GitAccess;
use crate::quackpack::core::solver::solver_freeze::{SolverFreeze, SolverPackageFreeze};
use crate::quackpack::core::solver::solver_mode::SolverMode;
use crate::quackpack::core::solver::types_common::{ExpandedLocation, ExpandedPackage};
use crate::quackpack::core::solver::{ShouldRunSolverEngine, SolverGathererData};
use crate::quackpack::core::{PackageContext, PackageLoader, Version};
use crate::quackpack::schemas::registry;
use crate::util::path_ops_ext::PathOpsExt;
use crate::util::test_utils::setup_test;

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
        // SAFETY: Setup is single threaded, and `Env` in `DuckContext`, copies all envs.
        unsafe {
            std::env::set_var("DUCK_CACHE_DIR", dir.path());
        }
        let ctx = DuckContext::default();
        // SAFETY: Setup is single threaded, and `Env` in `DuckContext`, copies all envs.
        unsafe {
            std::env::remove_var("DUCK_CACHE_DIR");
        }
        (ctx, dir)
    };
    setup_test(setup)
}

fn prepare_manifest(contents: &str) -> (TempDir, PathBuf) {
    let dir = tempdir().unwrap();
    let manifest = dir.path().join(PackageLoader::MANIFEST_NAME);
    manifest.touch().unwrap();
    manifest.write(contents).unwrap();
    dir.path().try_fsync_dir().unwrap();
    (dir, manifest)
}

fn create_mock_server() -> MockServer {
    let server = MockServer::start();

    // Assets for not_pinned_registry test.
    let a1 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "a".into(),
            description: "".into(),
        },
        dependencies: [].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
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
        features: [("a".into(), vec![])].into(),
        profiles: HashMap::new(),
    };

    let b2 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(2, 0, 0),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "b".into(),
            description: "".into(),
        },
        dependencies: [].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    // Assets for not_pinned_registry test.
    server.mock(|when, then| {
        when.method(GET).path("/packages/a/1.0.0");
        then.status(200).json_body_obj(&a1);
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/b/2.0.0");
        then.status(200).json_body_obj(&b2);
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/a");
        then.status(200).json_body_obj(&types::MultiMetadata {
            packages_metadata: vec![a1, a2],
        });
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/b");
        then.status(200).json_body_obj(&types::MultiMetadata {
            packages_metadata: vec![b2],
        });
    });

    server
}

#[test]
/// Tests a new dependency occuring in the manifest.
/// Main package depends on *a* and *b*, but only *a* is present in the supplied freeze.
fn new_dependency() {
    let (ctx, _root) = setup_duck_ctx();
    let server = create_mock_server();

    let url: Url = server.base_url().parse().unwrap();
    let mut fetcher = Fetcher::new(&ctx).unwrap();
    let (_dir, manifest_path) = prepare_manifest(&format!(
        r#"
metadata:
  name: root
  version: '0.1.0'

dependencies:
  a:
    source:
      registry-url: {}
    version: '1'
  b:
    source:
      registry-url: {}
    version: '2'
"#,
        &url, &url,
    ));
    let root_path = manifest_path.parent().unwrap().to_path_buf();
    let pcx = PackageContext::new(root_path.clone(), &ctx).unwrap();

    let loc_root = ExpandedLocation::Local {
        absolute_path: root_path.clone(),
    }
    .into();
    let root_pkg = ExpandedPackage {
        location: loc_root,
        version: None,
    };

    let loc_a = ExpandedLocation::Registry {
        url: url.clone(),
        real_name: "a".into(),
    }
    .into();
    let a_pkg = ExpandedPackage {
        location: loc_a,
        version: Some(1.into()),
    };

    let loc_b = ExpandedLocation::Registry {
        url: url.clone(),
        real_name: "b".into(),
    }
    .into();
    let b_pkg = ExpandedPackage {
        location: loc_b,
        version: Some(2.into()),
    };

    let previous_freeze = SolverFreeze {
        main_pkg: root_pkg,
        package_freezes: [
            (
                root_pkg,
                SolverPackageFreeze {
                    dependencies_realization: [("a".into(), a_pkg)].into(),
                    features: [].into(),
                },
            ),
            (
                a_pkg,
                SolverPackageFreeze {
                    dependencies_realization: [].into(),
                    features: [].into(),
                },
            ),
        ]
        .into(),
    };

    let solver = SolverGathererData::new(&pcx, previous_freeze, SolverMode::default());
    let ShouldRunSolverEngine::Yes(solver) = solver
        .prepare_solving(&mut fetcher, &mut MockGitAccess())
        .unwrap()
    else {
        panic!()
    };
    let new_freeze = solver.solve().unwrap().new_freeze;
    assert!(new_freeze.main_pkg == root_pkg);
    assert!(
        new_freeze.package_freezes
            == [
                (
                    root_pkg,
                    SolverPackageFreeze {
                        dependencies_realization: [("a".into(), a_pkg), ("b".into(), b_pkg)].into(),
                        features: [].into()
                    }
                ),
                (
                    a_pkg,
                    SolverPackageFreeze {
                        dependencies_realization: [].into(),
                        features: [].into()
                    }
                ),
                (
                    b_pkg,
                    SolverPackageFreeze {
                        dependencies_realization: [].into(),
                        features: [].into()
                    }
                ),
            ]
            .into()
    )
}

#[test]
/// Tests a dependency becoming unnecessary.
/// Main package depends on *b*, but both *a* and *b* are present in the supplied freeze.
fn remove_unnecessary_dependency() {
    let (ctx, _root) = setup_duck_ctx();
    let server = create_mock_server();

    let url: Url = server.base_url().parse().unwrap();
    let mut fetcher = Fetcher::new(&ctx).unwrap();
    let (_dir, manifest_path) = prepare_manifest(&format!(
        r#"
metadata:
  name: root
  version: '0.1.0'

dependencies:
  b:
    source:
      registry-url: {}
    version: '2'
"#,
        &url,
    ));
    let root_path = manifest_path.parent().unwrap().to_path_buf();
    let pcx = PackageContext::new(root_path.clone(), &ctx).unwrap();

    let loc_root = ExpandedLocation::Local {
        absolute_path: root_path.clone(),
    }
    .into();
    let root_pkg = ExpandedPackage {
        location: loc_root,
        version: None,
    };

    let loc_a = ExpandedLocation::Registry {
        url: url.clone(),
        real_name: "a".into(),
    }
    .into();
    let a_pkg = ExpandedPackage {
        location: loc_a,
        version: Some(1.into()),
    };

    let loc_b = ExpandedLocation::Registry {
        url: url.clone(),
        real_name: "b".into(),
    }
    .into();
    let b_pkg = ExpandedPackage {
        location: loc_b,
        version: Some(2.into()),
    };

    let previous_freeze = SolverFreeze {
        main_pkg: root_pkg,
        package_freezes: [
            (
                root_pkg,
                SolverPackageFreeze {
                    dependencies_realization: [("a".into(), a_pkg), ("b".into(), b_pkg)].into(),
                    features: [].into(),
                },
            ),
            (
                a_pkg,
                SolverPackageFreeze {
                    dependencies_realization: [].into(),
                    features: [].into(),
                },
            ),
            (
                b_pkg,
                SolverPackageFreeze {
                    dependencies_realization: [].into(),
                    features: [].into(),
                },
            ),
        ]
        .into(),
    };

    let solver = SolverGathererData::new(&pcx, previous_freeze, SolverMode::default());
    let ShouldRunSolverEngine::No(answer) = solver
        .prepare_solving(&mut fetcher, &mut MockGitAccess())
        .unwrap()
    else {
        panic!()
    };
    assert!(answer.new_freeze.main_pkg == root_pkg);
    assert!(
        answer.new_freeze.package_freezes
            == [
                (
                    root_pkg,
                    SolverPackageFreeze {
                        dependencies_realization: [("b".into(), b_pkg)].into(),
                        features: [].into()
                    }
                ),
                (
                    b_pkg,
                    SolverPackageFreeze {
                        dependencies_realization: [].into(),
                        features: [].into()
                    }
                ),
            ]
            .into()
    )
}

#[test]
/// Tests that when supplied freeze realization is not correct, a new, correct realization is chosen.
/// Main package depends on *a* with feature *a*, supplied freeze has *a* in version 1.0.0,
/// which does not have the requested feature.
/// Only *a* in version 2.0.0 has that feature and should be chosen to the new freeze.
///
/// Note:
/// [`SolverMode::Merciful`] is used in this test.
fn no_longer_working_dependency() {
    let (ctx, _root) = setup_duck_ctx();
    let server = create_mock_server();

    let url: Url = server.base_url().parse().unwrap();
    let mut fetcher = Fetcher::new(&ctx).unwrap();
    let (_dir, manifest_path) = prepare_manifest(&format!(
        r#"
metadata:
  name: root
  version: '0.1.0'

dependencies:
  a:
    source:
      registry-url: {}
    version: 1 or 2
    features: [a]
"#,
        &url,
    ));
    let root_path = manifest_path.parent().unwrap().to_path_buf();
    let pcx = PackageContext::new(root_path.clone(), &ctx).unwrap();

    let loc_root = ExpandedLocation::Local {
        absolute_path: root_path.clone(),
    }
    .into();
    let root_pkg = ExpandedPackage {
        location: loc_root,
        version: None,
    };

    let loc_a = ExpandedLocation::Registry {
        url: url.clone(),
        real_name: "a".into(),
    }
    .into();
    let a1_pkg = ExpandedPackage {
        location: loc_a,
        version: Some(1.into()),
    };
    let a2_pkg = ExpandedPackage {
        location: loc_a,
        version: Some(2.into()),
    };

    let previous_freeze = SolverFreeze {
        main_pkg: root_pkg,
        package_freezes: [
            (
                root_pkg,
                SolverPackageFreeze {
                    dependencies_realization: [("a".into(), a1_pkg)].into(),
                    features: [].into(),
                },
            ),
            (
                a1_pkg,
                SolverPackageFreeze {
                    dependencies_realization: [].into(),
                    features: [].into(),
                },
            ),
        ]
        .into(),
    };

    let mode = SolverMode {
        supress_foreign_manifests_errors: true,
        frozen: false,
    };
    let solver = SolverGathererData::new(&pcx, previous_freeze, mode);
    let ShouldRunSolverEngine::Yes(solver) = solver
        .prepare_solving(&mut fetcher, &mut MockGitAccess())
        .unwrap()
    else {
        panic!()
    };
    let new_freeze = solver.solve().unwrap().new_freeze;
    assert!(new_freeze.main_pkg == root_pkg);
    assert!(
        new_freeze.package_freezes
            == [
                (
                    root_pkg,
                    SolverPackageFreeze {
                        dependencies_realization: [("a".into(), a2_pkg)].into(),
                        features: [].into()
                    }
                ),
                (
                    a2_pkg,
                    SolverPackageFreeze {
                        dependencies_realization: [].into(),
                        features: ["a".into()].into()
                    }
                ),
            ]
            .into()
    )
}
