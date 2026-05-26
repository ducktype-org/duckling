use std::collections::HashMap;
use std::env::home_dir;
use std::path::PathBuf;

use tempfile::{TempDir, tempdir};

use super::parse_manifest;
use crate::quackpack::core::manifest::parse::frontmatter::parse_frontmatter;
use crate::quackpack::core::{BranchOrTag, OptLevel, Profile, Source, Version};
use crate::util::path_ops_ext::PathOpsExt;
use crate::{DuckContext, QpContext, StrId};

fn prepare_manifest(contents: &str) -> (TempDir, PathBuf) {
    let dir = tempdir().unwrap();
    let manifest = dir.path().join("x");
    manifest.touch().unwrap();
    manifest.write(contents).unwrap();
    dir.path().try_fsync_dir().unwrap();
    (dir, manifest)
}

fn prepare_frontmatter(contents: &str) -> (TempDir, PathBuf) {
    let dir = tempdir().unwrap();
    let frontmatter = dir.path().join("x");
    frontmatter.touch().unwrap();
    frontmatter
        .write(format!("<frontmatter>\n{}\n</frontmatter>", contents))
        .unwrap();
    dir.path().try_fsync_dir().unwrap();
    (dir, frontmatter)
}

fn make_errors_message<const N: usize>(root: &TempDir, errors: [&str; N]) -> String {
    let mut vec = [format!(
        "when trying to parse the user manifest at `{}/x`",
        root.path().display()
    )]
    .to_vec();
    vec.extend(errors.iter().map(|&x| String::from(x)));
    vec.join("\n")
}

fn make_errors_message_frontmatter<const N: usize>(root: &TempDir, errors: [&str; N]) -> String {
    let mut vec = [format!(
        "when trying to parse the frontmatter of the script at `{}/x`",
        root.path().display()
    )]
    .to_vec();
    vec.extend(errors.iter().map(|&x| String::from(x)));
    vec.join("\n")
}

#[test]
fn parse_metadata() {
    let (_dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'
"#,
    );
    let ctx = DuckContext::default();
    let manifest = parse_manifest(&manifest_path, &ctx).unwrap();
    let summary = manifest.manifest();
    assert_eq!(summary.name(), "xd");
    assert!(summary.dependencies().all_dependencies().is_empty());
    assert!(summary.dev_dependencies().all_dependencies().is_empty());
    assert!(summary.features().all_features().is_empty());
}

#[test]
fn parse_with_deps() {
    let (_dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'

dependencies:
  a:
    version: '0.1'
"#,
    );
    let ctx = DuckContext::default();
    let manifest = parse_manifest(&manifest_path, &ctx).unwrap();
    let summary = manifest.manifest();
    assert_eq!(summary.dependencies().all_dependencies().len(), 1);
    assert!(summary.dependencies().has_by_name(StrId::new("a")));
    let a = summary.dependencies().get_by_name(StrId::new("a")).unwrap();
    assert_eq!(a.versions().len(), 1);
    assert_eq!(a.versions()[0].to_string(), "0.1.0");
    assert!(a.source().is_registry());
    assert!(a.features().is_empty());
    assert!(summary.dev_dependencies().all_dependencies().is_empty());
    assert!(summary.features().all_features().is_empty());
}

#[test]
fn parse_with_integer_dep_version() {
    let (_dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'

dependencies:
  a:
    version: '1'
"#,
    );
    let ctx = DuckContext::default();
    let manifest = parse_manifest(&manifest_path, &ctx).unwrap();
    let summary = manifest.manifest();
    assert_eq!(summary.dependencies().all_dependencies().len(), 1);
    assert!(summary.dependencies().has_by_name(StrId::new("a")));
    let a = summary.dependencies().get_by_name(StrId::new("a")).unwrap();
    assert_eq!(a.versions().len(), 1);
    assert_eq!(a.versions()[0].to_string(), "1.0.0");
    assert!(a.source().is_registry());
    assert!(a.features().is_empty());
    assert!(summary.dev_dependencies().all_dependencies().is_empty());
    assert!(summary.features().all_features().is_empty());
}

#[test]
fn parse_with_negative_dep_version() {
    let (dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'

dependencies:
  a:
    version: '-1'
"#,
    );
    let ctx = DuckContext::default();
    let err = parse_manifest(&manifest_path, &ctx).unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message(
            &dir,
            ["dependencies.a.version: invalid digit found in string at line 8 column 14"]
        )
    );
}

#[test]
fn parse_with_dep_or_versions() {
    let (_dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'

dependencies:
  a:
    version: 0.1 or 2 # Here it's not needed, because `or` makes it implicitly a string...
"#,
    );
    let ctx = DuckContext::default();

    let manifest = parse_manifest(&manifest_path, &ctx).unwrap();
    let summary = manifest.manifest();
    assert_eq!(summary.dependencies().all_dependencies().len(), 1);
    let a = summary.dependencies().get_by_name(StrId::new("a")).unwrap();
    assert_eq!(a.versions().len(), 2);
    assert_eq!(a.versions()[0].to_string(), "0.1.0");
    assert_eq!(a.versions()[1].to_string(), "2.0.0");
    assert!(a.source().is_registry());
    assert!(a.features().is_empty());
    assert!(summary.dev_dependencies().all_dependencies().is_empty());
    assert!(summary.features().all_features().is_empty());
}

#[test]
fn parse_with_git_dep() {
    let (_dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'

dependencies:
  a:
    version: 0.1 or 2
    source:
      git-url: https://google.com
"#,
    );
    let ctx = DuckContext::default();
    let manifest = parse_manifest(&manifest_path, &ctx).unwrap();
    let summary = manifest.manifest();
    assert_eq!(summary.dependencies().all_dependencies().len(), 1);
    let a = summary.dependencies().get_by_name(StrId::new("a")).unwrap();
    assert_eq!(a.versions().len(), 2);
    assert_eq!(a.versions()[0].to_string(), "0.1.0");
    assert_eq!(a.versions()[1].to_string(), "2.0.0");
    assert!(a.source().is_git());
    if let Source::Git(git_source) = a.source().as_ref() {
        assert_eq!(git_source.url().as_str(), "https://google.com/");
    }
    assert!(summary.dev_dependencies().all_dependencies().is_empty());
    assert!(summary.features().all_features().is_empty());
}

#[test]
fn parse_with_extra_fields_fail() {
    let (dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'

dependencies:
  a:
    version: 0.1 or 2
    source:
      tag: xd
"#,
    );
    let ctx = DuckContext::default();
    let result = parse_manifest(&manifest_path, &ctx);
    assert!(result.is_err());
    let err = result.unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message(
            &dir,
            [
                "expected the dependency `dependencies.a` to not be a git dependency, \
       but the field `dependencies.a.source.tag` is set"
            ]
        )
    );
}

#[test]
fn fail_registry_without_version() {
    let (dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'

dependencies:
  a:
    source:
      registry-url: https://google.com
"#,
    );
    let ctx = DuckContext::default();
    let result = parse_manifest(&manifest_path, &ctx);
    assert!(result.is_err());
    let err = result.unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message(
            &dir,
            [
                "when parsing the field `dependencies.a`",
                "a registry dependency must provide at least one version",
            ]
        )
    );
}

#[test]
fn parse_local_dep_with_versions() {
    let (dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'

dependencies:
  a:
    version: '0.1'
    source:
      path: xd
"#,
    );
    let ctx = DuckContext::default();
    let result = parse_manifest(&manifest_path, &ctx);
    assert!(result.is_err());
    let err = result.unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message(
            &dir,
            [
                "couldn't determine the type of the dependency `dependencies.a`
remove one of the fields `dependencies.a.version` or `dependencies.a.source.path`"
            ]
        )
    )
}

#[test]
fn fail_no_metadata() {
    let (dir, manifest_path) = prepare_manifest(
        r#"
dependencies:
  a:
    source:
      path: xd
"#,
    );
    let ctx = DuckContext::default();
    let result = parse_manifest(&manifest_path, &ctx);
    assert!(result.is_err());
    let err = result.unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message(&dir, ["missing the obligatory section `metadata`"])
    )
}

#[test]
fn fail_no_metadata_name() {
    let (dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  version: '0.1'

dependencies:
  a:
    source:
      path: xd
"#,
    );
    let ctx = DuckContext::default();
    let result = parse_manifest(&manifest_path, &ctx);
    assert!(result.is_err());
    let err = result.unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message(&dir, ["missing the obligatory key `metadata.name`"])
    )
}

#[test]
fn fail_no_metadata_version() {
    let (dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd

dependencies:
  a:
    source:
      path: xd
"#,
    );
    let ctx = DuckContext::default();
    let result = parse_manifest(&manifest_path, &ctx);
    assert!(result.is_err());
    let err = result.unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message(&dir, ["missing the obligatory key `metadata.version`"])
    )
}

#[test]
fn parse_deps_sources() {
    let (_dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'

dependencies:
  a:
    source:
      path: xd
  a1:
    source:
      path: ../xd
  a2:
    source:
      path: ~/xd
  a3:
    source:
      path: /xd
  b:
    version: '0.1'
  c:
    version: '0.1'
    source:
      name: alias
  d:
    version: '0.1'
    source:
      name: alias2
      registry-url: https://google.com
  e:
    source:
      git-url: https://google.com
      branch: branch
      commit: commit
"#,
    );
    let ctx = DuckContext::default();
    let manifest = parse_manifest(&manifest_path, &ctx).unwrap();
    let summary = manifest.manifest();
    assert_eq!(summary.dependencies().all_dependencies().len(), 8);

    let a = summary.dependencies().get_by_name(StrId::new("a")).unwrap();
    assert!(a.source().is_local());
    if let Source::Local(local_source) = a.source().as_ref() {
        assert_eq!(
            local_source.absolute(),
            manifest_path
                .parent()
                .unwrap()
                .resolve()
                .unwrap()
                .join("xd")
        );
        assert!(local_source.was_original_entry_relative());
        assert_eq!(local_source.entry_in_manifest(), "xd");
    }
    assert!(a.versions().is_empty());
    assert_eq!(a.name(), a.effective_name());
    assert!(a.alias().is_none());

    let a1 = summary
        .dependencies()
        .get_by_name(StrId::new("a1"))
        .unwrap();
    assert!(a1.source().is_local());
    if let Source::Local(local_source) = a1.source().as_ref() {
        assert_eq!(
            local_source.absolute(),
            manifest_path
                .parent()
                .unwrap()
                .parent()
                .unwrap()
                .resolve()
                .unwrap()
                .join("xd")
        );
        assert!(local_source.was_original_entry_relative());
        assert_eq!(local_source.entry_in_manifest(), "../xd");
    }
    assert!(a1.alias().is_none());

    let a2 = summary
        .dependencies()
        .get_by_name(StrId::new("a2"))
        .unwrap();
    assert!(a2.source().is_local());
    if let Source::Local(local_source) = a2.source().as_ref() {
        let home_dir = home_dir().unwrap();
        assert_eq!(local_source.absolute(), home_dir.join("xd"));
        assert!(!local_source.was_original_entry_relative());
        assert_eq!(local_source.entry_in_manifest(), "~/xd");
    }
    assert!(a2.alias().is_none());

    let a3 = summary
        .dependencies()
        .get_by_name(StrId::new("a3"))
        .unwrap();
    assert!(a3.source().is_local());
    if let Source::Local(local_source) = a3.source().as_ref() {
        assert_eq!(local_source.absolute(), PathBuf::from("/xd"));
        assert!(!local_source.was_original_entry_relative());
        assert_eq!(local_source.entry_in_manifest(), "/xd");
    }
    assert!(a3.alias().is_none());

    let b = summary.dependencies().get_by_name(StrId::new("b")).unwrap();
    assert!(b.source().is_registry());
    if let Source::Registry(registry_source) = b.source().as_ref() {
        let default_registry = ctx.registry_url().unwrap();
        assert_eq!(*registry_source.url(), default_registry);
    }
    assert_eq!(b.versions().len(), 1);
    assert_eq!(b.versions()[0].to_string(), "0.1.0");
    assert_eq!(b.name(), b.effective_name());
    assert!(b.alias().is_none());

    let c = summary
        .dependencies()
        .get_by_alias(StrId::new("c"))
        .unwrap();
    assert!(c.source().is_registry());
    assert_eq!(c.versions().len(), 1);
    assert_eq!(c.name(), "alias");
    assert_eq!(c.alias(), Some("c".into()));
    assert_ne!(c.name(), c.effective_name());

    let d = summary
        .dependencies()
        .get_by_alias(StrId::new("d"))
        .unwrap();
    assert!(d.source().is_registry());
    if let Source::Registry(registry_source) = d.source().as_ref() {
        assert_eq!(registry_source.url().as_str(), "https://google.com/");
    }
    assert_eq!(d.versions().len(), 1);
    assert_eq!(d.name(), "alias2");
    assert_eq!(d.alias(), Some("d".into()));
    assert_ne!(d.name(), d.effective_name());

    let e = summary.dependencies().get_by_name(StrId::new("e")).unwrap();
    assert!(e.source().is_git());
    if let Source::Git(git_source) = e.source().as_ref() {
        assert_eq!(git_source.url().as_str(), "https://google.com/");
        assert_eq!(
            git_source.branch_or_tag(),
            &BranchOrTag::Branch("branch".to_owned())
        );
        assert_eq!(git_source.rev().as_deref(), Some("commit"));
    }
    assert!(e.versions().is_empty());
    assert_eq!(e.name(), e.effective_name());
    assert!(b.alias().is_none());
}

#[test]
fn fail_exclusive_git_fields() {
    let (dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'

dependencies:
  a:
    version: '0.1'
    source:
      git-url: git
      tag: tag
      branch: branch
"#,
    );
    let ctx = DuckContext::default();
    let result = parse_manifest(&manifest_path, &ctx);
    assert!(result.is_err());
    let err = result.unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message(
            &dir,
            [
                "the dependency `dependencies.a.source` is a git dependency, but it contains mutually exclusive fields: \
                  `dependencies.a.source.branch`, `dependencies.a.source.tag`"
            ]
        )
    );
}

#[test]
fn features() {
    let (_dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'

features:
  a: []
"#,
    );
    let ctx = DuckContext::default();
    let manifest = parse_manifest(&manifest_path, &ctx).unwrap();
    let summary = manifest.manifest();
    assert_eq!(summary.features().all_features().len(), 1);
    let a_feature = summary
        .features()
        .all_features()
        .iter()
        .find(|(id, _)| *id == "a");
    assert!(a_feature.is_some());
    assert!(a_feature.unwrap().1.is_empty());
}

#[test]
fn features_expansion() {
    let (_dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'

features:
  a: [b]
  b: [c, d]
  c: []
  d: []
"#,
    );
    let ctx = DuckContext::default();
    let manifest = parse_manifest(&manifest_path, &ctx).unwrap();
    let summary = manifest.manifest();
    assert_eq!(summary.features().all_features().len(), 4);

    // Find feature IDs
    let a = summary
        .features()
        .all_features()
        .keys()
        .find(|id| *id == "a")
        .unwrap();
    let b = summary
        .features()
        .all_features()
        .keys()
        .find(|id| *id == "b")
        .unwrap();
    let c = summary
        .features()
        .all_features()
        .keys()
        .find(|id| *id == "c")
        .unwrap();
    let d = summary
        .features()
        .all_features()
        .keys()
        .find(|id| *id == "d")
        .unwrap();

    assert_eq!(summary.features().all_features()[a].len(), 1);
    assert_eq!(summary.features().all_features()[b].len(), 2);
    assert!(summary.features().all_features()[c].is_empty());
    assert!(summary.features().all_features()[d].is_empty());

    // Test feature expansion
    let expanded_a = summary.features().expand_features([*a]).unwrap();
    assert_eq!(expanded_a.len(), 4);
    assert!(expanded_a.contains(a));
    assert!(expanded_a.contains(b));
    assert!(expanded_a.contains(c));
    assert!(expanded_a.contains(d));

    let expanded_b = summary.features().expand_features([*b]).unwrap();
    assert_eq!(expanded_b.len(), 3);
    assert!(expanded_b.contains(b));
    assert!(expanded_b.contains(c));
    assert!(expanded_b.contains(d));
}

#[test]
fn dep_features() {
    let (_dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'

dependencies:
  a:
    version: '0.1'
    features: [a, b]
"#,
    );
    let ctx = DuckContext::default();
    let manifest = parse_manifest(&manifest_path, &ctx).unwrap();
    let summary = manifest.manifest();
    let dep = summary.dependencies().get_by_name(StrId::new("a")).unwrap();

    assert_eq!(dep.features().len(), 2);
    let feature_names: Vec<String> = dep
        .features()
        .iter()
        .map(|f| f.name().to_string())
        .collect();
    assert!(feature_names.contains(&"a".to_string()));
    assert!(feature_names.contains(&"b".to_string()));

    let enabled = dep.enabled_features(vec![]);
    assert_eq!(enabled.len(), 2);
}

#[test]
fn dep_features_with_conds() {
    let (_dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'

dependencies:
  a:
    version: '0.1'
    features:
      - a
      -
        b:
          package-features:
            - a
      - c
"#,
    );
    let ctx = DuckContext::default();
    let manifest = parse_manifest(&manifest_path, &ctx).unwrap();
    let summary = manifest.manifest();
    let dep = summary.dependencies().get_by_name(StrId::new("a")).unwrap();

    assert_eq!(dep.features().len(), 3);
    let feature_names: Vec<String> = dep
        .features()
        .iter()
        .map(|f| f.name().to_string())
        .collect();
    assert!(feature_names.contains(&"a".to_string()));
    assert!(feature_names.contains(&"b".to_string()));
    assert!(feature_names.contains(&"c".to_string()));

    let enabled_features = dep.enabled_features(vec![]);

    assert!(enabled_features.contains(&StrId::new("a")));
}

#[test]
fn empty_conditions_features() {
    let (dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'

dependencies:
  a:
    version: '0.1'
    conditions:
      package-features: []
"#,
    );
    let ctx = DuckContext::default();
    let result = parse_manifest(&manifest_path, &ctx);
    assert!(result.is_err());
    let err = result.unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message(
            &dir,
            [
                "when parsing the field `dependencies.a.conditions`",
                "the field `package-features` is present but empty, if you don't want to specify it, remove it from the manifest"
            ]
        )
    );
}

#[test]
fn dep_features_with_invalid_conds() {
    let (dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'

dependencies:
  a:
    version: '0.1'
    features:
      -
        b:
          package-features:
            - a
        c:
"#,
    );
    let ctx = DuckContext::default();
    let err = parse_manifest(&manifest_path, &ctx).unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message(
            &dir,
            [
                "dependencies.a.features[0]: invalid length 0, expected a map with exactly one entry at line 11 column 9",
            ]
        )
    );

    let (dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.1'

dependencies:
  a:
    version: '0.1'
    features:
      - {}
"#,
    );
    let ctx = DuckContext::default();
    let err = parse_manifest(&manifest_path, &ctx).unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message(
            &dir,
            [
                "dependencies.a.features[0]: invalid length 0, expected a map with exactly one entry at line 10 column 9",
            ]
        )
    );
}

#[test]
fn git_url_points_to_local_dir() {
    let root_dir = TempDir::new().unwrap();
    let (dir, manifest_path) = prepare_manifest(&format!(
        r#"
metadata:
  name: xd
  version: '0.1'

dependencies:
  a:
    source:
      git-url: {}
"#,
        root_dir.path().display()
    ));
    let ctx = DuckContext::default();

    let err = parse_manifest(&manifest_path, &ctx).unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message(
            &dir,
            [
                &format!("`{}` is not a valid URL", root_dir.path().display()),
                "git dependency points to a file on the disk",
                &format!(
                    "either change it to a local dependency or change the URL to `file://{}`",
                    root_dir.path().display()
                ),
                "relative URL without a base",
            ]
        )
    );
}

#[test]
fn valid_git_url_points_to_local_dir() {
    let root_dir = TempDir::new().unwrap();
    let (_dir, manifest_path) = prepare_manifest(&format!(
        r#"
metadata:
  name: xd
  version: '0.1'

dependencies:
  a:
    source:
      git-url: file://{}
"#,
        root_dir.path().display()
    ));
    let ctx = DuckContext::default();

    assert!(parse_manifest(&manifest_path, &ctx).is_ok());
}

#[test]
fn floats_explicit_string_dont_work() {
    let (dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.10'

dependencies:
  a:
    version: '0.10'
  b:
    version: ['0.10', 0.10]
"#,
    );
    let ctx = DuckContext::default();
    let err = parse_manifest(&manifest_path, &ctx).unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message(
            &dir,
            [
                "dependencies.b.version[1]: invalid type: floating point `0.1`, expected a semver string at line 10 column 23"
            ]
        )
    );
}

#[test]
fn floats_dont_parse() {
    let (dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.10'

dependencies:
  a:
    version: 0.10
"#,
    );
    let ctx = DuckContext::default();
    let err = parse_manifest(&manifest_path, &ctx).unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message(
            &dir,
            [
                "dependencies.a.version: invalid type: floating point `0.1`, expected an ored semver string or a list of semver strings at line 8 column 14"
            ]
        )
    );
}

#[test]
fn floats_root_version_parses() {
    let (_dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.10'
"#,
    );
    let ctx = DuckContext::default();
    let manifest = parse_manifest(&manifest_path, &ctx).unwrap();
    let summary = manifest.manifest();
    assert_eq!(summary.version(), Version::new(0, 10, 0));
}

#[test]
fn profiles_parse() {
    let (_dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.10'

profiles:
  prof1:
    opt-level: s
    dvm-bytecode: true
  prof2:
    inherits: prof1
"#,
    );
    let ctx = DuckContext::default();
    let manifest = parse_manifest(&manifest_path, &ctx).unwrap();
    let summary = manifest.manifest();
    let profiles = summary.profiles();
    assert!(
        *profiles.get_profiles()
            == HashMap::from([
                (
                    "prof1".into(),
                    Profile {
                        opt_level: Some(OptLevel::S),
                        dvm_bytecode: Some(true),
                        incremental: None,
                        c_std: None,
                        inherits: None
                    }
                ),
                (
                    "prof2".into(),
                    Profile {
                        opt_level: None,
                        dvm_bytecode: None,
                        incremental: None,
                        c_std: None,
                        inherits: Some("prof1".into())
                    }
                )
            ])
    )
}

#[test]
fn unknown_opt_level() {
    let (dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.10'

profiles:
  prof1:
    opt-level: x
"#,
    );
    let ctx = DuckContext::default();
    let err = parse_manifest(&manifest_path, &ctx).unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message(
            &dir,
            [
                "when parsing the field `profiles`",
                "when parsing the field `profiles.prof1`",
                "Unknown optimization level `x`. Optimization levels are 0, 1, 2, 3, s (or S), z (or Z)."
            ]
        )
    );
}

#[test]
fn duplicated_names() {
    let (dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.10'

dependencies:
  a:
    version: '1'
  b:
    version: '1'
    source:
      name: a
"#,
    );
    let ctx = DuckContext::default();
    let err = parse_manifest(&manifest_path, &ctx).unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message(
            &dir,
            [
                "when parsing the field `dependencies`",
                "multiple dependencies specify the same name `a`"
            ]
        )
    );
}

#[test]
fn duplicated_names_in_aliases() {
    let (dir, manifest_path) = prepare_manifest(
        r#"
metadata:
  name: xd
  version: '0.10'

dependencies:
  a:
    version: '1'
    source:
      name: c
  b:
    version: '1'
    source:
      name: c
"#,
    );
    let ctx = DuckContext::default();
    let err = parse_manifest(&manifest_path, &ctx).unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message(
            &dir,
            [
                "when parsing the field `dependencies`",
                "multiple dependencies specify the same name `c`"
            ]
        )
    );
}

#[test]
fn frontmatter() {
    let (_dir, frontmatter_path) = prepare_frontmatter(
        r#"
dependencies:
  a:
    version: '1'
"#,
    );
    let ctx = DuckContext::default();
    let frontmatter = parse_frontmatter(frontmatter_path, &ctx).unwrap().unwrap();
    let _ = frontmatter
        .dependencies()
        .get_by_name(StrId::new("a"))
        .unwrap()
        .clone();
    assert_eq!(frontmatter.dependencies().all_dependencies().len(), 1);
    assert_eq!(frontmatter.dev_dependencies().all_dependencies().len(), 0);
    assert_eq!(frontmatter.profiles().get_profiles().len(), 0);
}

#[test]
fn fails_frontmatter_with_import_and_other_fields() {
    let (dir, frontmatter_path) = prepare_frontmatter(
        r#"
import: xd
dependencies:
  a:
    version: '1'
"#,
    );
    let ctx = DuckContext::default();
    let err = parse_frontmatter(frontmatter_path.clone(), &ctx).unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message_frontmatter(
            &dir,
            [
                "either remove the `import` field or all the other fields",
                &format!(
                    "script at `{}` imports a frontmatter but also specifies some of the frontmatter fields",
                    frontmatter_path.display()
                ),
            ]
        )
    );
}

#[test]
fn frontmatter_with_import() {
    let dir = TempDir::new().unwrap();
    let importing = dir.path().join("x");
    let imported = dir.path().join("y");
    importing.touch().unwrap();
    importing
        .write(
            r#"
<frontmatter>
import: y
</frontmatter>
        "#,
        )
        .unwrap();
    imported.touch().unwrap();
    imported
        .write(
            r#"
dependencies:
  a:
    version: '1'
        "#,
        )
        .unwrap();
    let ctx = DuckContext::default();
    let frontmatter = parse_frontmatter(importing, &ctx).unwrap().unwrap();
    let _ = frontmatter
        .dependencies()
        .get_by_name(StrId::new("a"))
        .unwrap()
        .clone();
    assert_eq!(frontmatter.dependencies().all_dependencies().len(), 1);
    assert_eq!(frontmatter.dev_dependencies().all_dependencies().len(), 0);
    assert_eq!(frontmatter.profiles().get_profiles().len(), 0);
}

#[test]
fn fails_frontmatter_import_not_existing() {
    let (dir, frontmatter_path) = prepare_frontmatter(
        r#"
import: y
"#,
    );
    let ctx = DuckContext::default();
    let err = parse_frontmatter(frontmatter_path.clone(), &ctx).unwrap_err();
    assert_eq!(
        err.to_string(),
        make_errors_message_frontmatter(
            &dir,
            [
                &format!(
                    "while reading the frontmatter imported by `{}` at `{}`",
                    frontmatter_path.display(),
                    dir.path().join("y").display(),
                ),
                &format!("failed to read `{}`", dir.path().join("y").display(),),
                "No such file or directory (os error 2)",
            ]
        )
    );
}
