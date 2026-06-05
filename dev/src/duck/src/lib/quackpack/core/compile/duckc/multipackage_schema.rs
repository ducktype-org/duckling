//! Schema of the multipackage JSON sent to duckc from QuackPack
//!
//! There are a few nuances we have to remember about:
//! * when compiling a package, we need to pass its __entire__ subtree to duckc,
//! * only ids specified in [`tasks`](MultiPackage::tasks) will be compiled; this effectively
//!   allows us to compile a single package, entire graph, or a chosen subset.

use std::path::PathBuf;

use serde::{Deserialize, Serialize, de, ser};

use crate::StrId;
use crate::quackpack::core::{FeatureName, Version};

#[derive(Default, Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
pub struct MultiPackage {
    /// Packages required for a successful duckc invocation; packages have to contain their __entire__ dependencies trees.
    pub packages: Vec<Package>,
    /// Duckc tasks to finish in this invocation.
    pub tasks: Vec<Task>,
}

#[derive(Default, Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
pub struct Package {
    #[serde(rename = "id")]
    /// ID of a package to compile. Note, that it doesn't have to be a package's name, it can be a
    /// unique id.
    /// Has to be unique in terms of the entire [`packages`](MultiPackage::packages) vector.
    pub id: StrId,
    #[serde(rename = "name")]
    /// Import name of the package (used when resolving imports in source code).
    /// Must be unique within the manifest.
    pub import_name: StrId,
    /// Version of the package we're currently compiling.
    pub version: Version,
    /// Enabled features for this package.
    pub features: Vec<FeatureName>,
    #[serde(rename = "path")]
    /// Path to the source directory of this package.
    pub path_to_the_src_directory: PathBuf,
    /// Dependencies of this package.
    pub dependencies: Vec<Dependency>,
}

#[derive(Default, Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
/// A single package's dependency, in a duckc-friendly format.
pub struct Dependency {
    #[serde(rename = "id")]
    /// ID of the dependency. Note, that there must a package with `id = self.id` in a [`packages`](MultiPackage::packages) vector.
    pub id: StrId,
    #[serde(skip_serializing_if = "Option::is_none")]
    /// How should this dependency be named when resolving imports. Defaults to the target package's `name`.
    pub alias: Option<StrId>,
}

// `compiler/driver/driver/src/driver/task/task.cpp` deserializes `RawTask` as `RawPackageCompilationTask`.
// (`compiler/driver/driver/src/driver/manifest/manifest.hpp`, which actually defines the JSON format,
// expects `std::vector<RawTask>`, so we actually want to know, how JSON should look).
// So, from JSON POV, they are the same type.
// (The referenced file is actually how duckc deserializes file, i.e. how it should look).
pub type Task = PackageCompilationTask;

#[derive(Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
pub struct PackageCompilationTask {
    #[serde(rename = "package")]
    /// ID of a package refered by this task.
    pub package_id: StrId,
    #[serde(flatten)]
    /// Compilation strategy of this task.
    pub strategy: PackageCompilationStrategy,
}

#[derive(Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
// These and the flatten above are required for generating valid JSON's, while still giving us type safety :^).
#[serde(rename_all = "snake_case", tag = "strategy")]
/// Supported duckc compilation strategies.
pub enum PackageCompilationStrategy {
    /// Compile this task into a DVM file.
    // Name has to be `Dvm`, as `DVM` (with "snake_case") would be rendered as "d_v_m".
    Dvm {
        /// Path to the output file.
        output_file: PathBuf,
    },
    /// Compile this task into a native binary.
    #[serde(rename = "native")]
    Binary {
        /// Path to the output file.
        output_file: PathBuf,
        /// Additional linking options.
        #[serde(skip_serializing_if = "Option::is_none")]
        linking_options: Option<LinkerOptions>,
    },
    /// Compile this task into a `.a` library. Note, that the produced artifacts might reference
    /// unresolved symbols (i.e. it can require additional produced `.a` libraries during linking).
    Lib {
        /// Path to the output file.
        output_file: PathBuf,
        /// Additional archiving options.
        #[serde(skip_serializing_if = "Option::is_none")]
        archive_options: Option<ArchiverOptions>,
    },
    /// Compile to an LLVM object (`.o` file).
    #[serde(rename = "obj")]
    EmitLLVMObject {},
}

#[derive(Debug, Clone, Eq, PartialEq, Hash)]
/// Possible variants of linking options passed to duckc.
pub enum LinkerOptions {
    /// Add extra linker arguments.
    RawLinkerArgs(String),
    Complex(ComplexLinkerOptions),
}

impl ser::Serialize for LinkerOptions {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: ser::Serializer,
    {
        match self {
            Self::RawLinkerArgs(raw) => raw.serialize(serializer),
            Self::Complex(complex) => complex.serialize(serializer),
        }
    }
}

impl<'de> de::Deserialize<'de> for LinkerOptions {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        serde_untagged::UntaggedEnumVisitor::new()
            .expecting("a valid duckc linking options")
            .string(|string| Ok(Self::RawLinkerArgs(string.to_string())))
            .map(|map| map.deserialize().map(Self::Complex))
            .deserialize(deserializer)
    }
}

#[derive(Default, Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
/// More complex linking options.
pub struct ComplexLinkerOptions {
    /// Use an explicit linker, instead of the system default.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub linker: Option<PathBuf>,
    /// Pass additional linker arguments.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub additional_linking_options: Option<String>,
    #[serde(
        rename = "link_c_standard_library",
        skip_serializing_if = "Option::is_none"
    )]
    /// Whether or not link C Std.
    pub link_cstd: Option<bool>,
}

#[derive(Debug, Clone, Eq, PartialEq, Hash)]
/// Possible variants of archiver options passed to duckc.
pub enum ArchiverOptions {
    /// Set an explicit archiver path.
    Archiver(PathBuf),
    Complex(ComplexArchiverOptions),
}

impl ser::Serialize for ArchiverOptions {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: ser::Serializer,
    {
        match self {
            Self::Archiver(raw) => raw.serialize(serializer),
            Self::Complex(complex) => complex.serialize(serializer),
        }
    }
}

impl<'de> de::Deserialize<'de> for ArchiverOptions {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        serde_untagged::UntaggedEnumVisitor::new()
            .expecting("a valid duckc archiver options")
            .string(|string| Ok(Self::Archiver(string.into())))
            .map(|map| map.deserialize().map(Self::Complex))
            .deserialize(deserializer)
    }
}

#[derive(Default, Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
/// More complex archiver options.
pub struct ComplexArchiverOptions {
    /// Set an explicit archiver.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub archiver: Option<PathBuf>,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn basic_json_convertion() {
        let packages = create_mock_packages();
        let tasks = create_mock_tasks();
        let multi_package = MultiPackage { packages, tasks };
        let serialized = serde_json::to_string_pretty(&multi_package).unwrap();
        assert_eq!(
            serialized,
            r#"{
  "packages": [
    {
      "id": "a",
      "name": "a",
      "version": "1.0.0",
      "features": [],
      "path": "",
      "dependencies": [
        {
          "id": "dep-hash",
          "alias": "bar"
        },
        {
          "id": "dep2-hash",
          "alias": "foo"
        }
      ]
    },
    {
      "id": "dep-hash",
      "name": "dep",
      "version": "2.0.0",
      "features": [
        "feature"
      ],
      "path": "",
      "dependencies": [
        {
          "id": "dep2-hash",
          "alias": "foo-aliased"
        }
      ]
    },
    {
      "id": "dep2-hash",
      "name": "dep2",
      "version": "2.1.37",
      "features": [
        "foo",
        "bar"
      ],
      "path": "",
      "dependencies": []
    }
  ],
  "tasks": [
    {
      "package": "a",
      "strategy": "native",
      "output_file": "out.exe",
      "linking_options": {
        "additional_linking_options": "-lfoo",
        "link_c_standard_library": true
      }
    },
    {
      "package": "a",
      "strategy": "native",
      "output_file": "out.exe",
      "linking_options": "-lfoo"
    },
    {
      "package": "a",
      "strategy": "native",
      "output_file": "out.exe"
    },
    {
      "package": "dep-hash",
      "strategy": "dvm",
      "output_file": "out.dvm"
    },
    {
      "package": "dep2-hash",
      "strategy": "lib",
      "output_file": "out.so",
      "archive_options": "ar"
    },
    {
      "package": "dep2-hash",
      "strategy": "lib",
      "output_file": "out.so",
      "archive_options": {
        "archiver": "ar"
      }
    },
    {
      "package": "dep2-hash",
      "strategy": "lib",
      "output_file": "out.so",
      "archive_options": {}
    },
    {
      "package": "dep2-hash",
      "strategy": "lib",
      "output_file": "out.so"
    }
  ]
}"#
        );

        let deserialized = serde_json::from_str::<MultiPackage>(&serialized).unwrap();
        assert_eq!(deserialized, multi_package);
    }

    fn create_mock_packages() -> Vec<Package> {
        vec![
            Package {
                id: "a".into(),
                import_name: "a".into(),
                version: Version::new(1, 0, 0),
                features: vec![],
                path_to_the_src_directory: PathBuf::default(),
                dependencies: vec![
                    Dependency {
                        id: "dep-hash".into(),
                        alias: Some("bar".into()),
                    },
                    Dependency {
                        id: "dep2-hash".into(),
                        alias: Some("foo".into()),
                    },
                ],
            },
            Package {
                id: "dep-hash".into(),
                import_name: "dep".into(),
                version: Version::new(2, 0, 0),
                features: vec!["feature".into()],
                path_to_the_src_directory: PathBuf::default(),
                dependencies: vec![Dependency {
                    id: "dep2-hash".into(),
                    alias: Some("foo-aliased".into()),
                }],
            },
            Package {
                id: "dep2-hash".into(),
                import_name: "dep2".into(),
                version: Version::new(2, 1, 37),
                features: vec!["foo".into(), "bar".into()],
                path_to_the_src_directory: PathBuf::default(),
                dependencies: vec![],
            },
        ]
    }

    fn create_mock_tasks() -> Vec<PackageCompilationTask> {
        vec![
            PackageCompilationTask {
                package_id: "a".into(),
                strategy: PackageCompilationStrategy::Binary {
                    output_file: PathBuf::from("out.exe"),
                    linking_options: Some(LinkerOptions::Complex(ComplexLinkerOptions {
                        linker: None,
                        additional_linking_options: Some("-lfoo".into()),
                        link_cstd: Some(true),
                    })),
                },
            },
            PackageCompilationTask {
                package_id: "a".into(),
                strategy: PackageCompilationStrategy::Binary {
                    output_file: PathBuf::from("out.exe"),
                    linking_options: Some(LinkerOptions::RawLinkerArgs("-lfoo".into())),
                },
            },
            PackageCompilationTask {
                package_id: "a".into(),
                strategy: PackageCompilationStrategy::Binary {
                    output_file: PathBuf::from("out.exe"),
                    linking_options: None,
                },
            },
            PackageCompilationTask {
                package_id: "dep-hash".into(),
                strategy: PackageCompilationStrategy::Dvm {
                    output_file: PathBuf::from("out.dvm"),
                },
            },
            PackageCompilationTask {
                package_id: "dep2-hash".into(),
                strategy: PackageCompilationStrategy::Lib {
                    output_file: PathBuf::from("out.so"),
                    archive_options: Some(ArchiverOptions::Archiver("ar".into())),
                },
            },
            PackageCompilationTask {
                package_id: "dep2-hash".into(),
                strategy: PackageCompilationStrategy::Lib {
                    output_file: PathBuf::from("out.so"),
                    archive_options: Some(ArchiverOptions::Complex(ComplexArchiverOptions {
                        archiver: Some("ar".into()),
                    })),
                },
            },
            PackageCompilationTask {
                package_id: "dep2-hash".into(),
                strategy: PackageCompilationStrategy::Lib {
                    output_file: PathBuf::from("out.so"),
                    archive_options: Some(ArchiverOptions::Complex(ComplexArchiverOptions {
                        archiver: None,
                    })),
                },
            },
            PackageCompilationTask {
                package_id: "dep2-hash".into(),
                strategy: PackageCompilationStrategy::Lib {
                    output_file: PathBuf::from("out.so"),
                    archive_options: None,
                },
            },
        ]
    }

    #[test]
    fn json_from_piotrek() {
        let packages = packages_from_piotrek();
        let tasks = tasks_from_piotrek();
        let multi_package = MultiPackage { packages, tasks };
        let serialized = serde_json::to_string_pretty(&multi_package).unwrap();
        assert_eq!(
            serialized,
            r#"{
  "packages": [
    {
      "id": "lib_a",
      "name": "lib_a",
      "version": "0.1.0",
      "features": [
        "use_mathlib"
      ],
      "path": "duck_modules/multi_package/lib_a",
      "dependencies": []
    },
    {
      "id": "lib_b",
      "name": "lib_b",
      "version": "0.1.0",
      "features": [
        "use_mathlib",
        "use_lib_a"
      ],
      "path": "duck_modules/multi_package/lib_b",
      "dependencies": [
        {
          "id": "lib_a",
          "alias": "lib_a"
        },
        {
          "id": "lib_c"
        }
      ]
    },
    {
      "id": "lib_c",
      "name": "lib_c",
      "version": "0.1.0",
      "features": [
        "use_lib_b"
      ],
      "path": "duck_modules/multi_package/lib_c",
      "dependencies": [
        {
          "id": "lib_b"
        }
      ]
    },
    {
      "id": "app1",
      "name": "app1",
      "version": "0.1.0",
      "features": [],
      "path": "duck_modules/multi_package/app1",
      "dependencies": [
        {
          "id": "lib_a",
          "alias": "lib_a"
        },
        {
          "id": "lib_c",
          "alias": "lib_b"
        }
      ]
    },
    {
      "id": "app2",
      "name": "app2",
      "version": "0.1.0",
      "features": [],
      "path": "duck_modules/multi_package/app2",
      "dependencies": [
        {
          "id": "lib_a",
          "alias": "lib_a"
        }
      ]
    },
    {
      "id": "app3",
      "name": "app3",
      "version": "0.1.0",
      "features": [],
      "path": "duck_modules/multi_package/app3",
      "dependencies": [
        {
          "id": "lib_a"
        },
        {
          "id": "lib_b"
        }
      ]
    },
    {
      "id": "app3_alias",
      "name": "app3_alias",
      "version": "0.1.0",
      "features": [],
      "path": "duck_modules/multi_package/app3",
      "dependencies": [
        {
          "id": "lib_a",
          "alias": "lib_b"
        },
        {
          "id": "lib_b",
          "alias": "lib_a"
        }
      ]
    }
  ],
  "tasks": [
    {
      "package": "lib_a",
      "strategy": "lib",
      "output_file": "lib_a"
    },
    {
      "package": "lib_b",
      "strategy": "lib",
      "output_file": "lib_b"
    },
    {
      "package": "lib_c",
      "strategy": "lib",
      "output_file": "lib_c"
    },
    {
      "package": "lib_a",
      "strategy": "dvm",
      "output_file": "lib_a_dvm"
    },
    {
      "package": "lib_b",
      "strategy": "dvm",
      "output_file": "lib_b_dvm"
    },
    {
      "package": "lib_c",
      "strategy": "dvm",
      "output_file": "lib_c_dvm"
    },
    {
      "package": "app1",
      "strategy": "dvm",
      "output_file": "app1_dvm"
    },
    {
      "package": "app2",
      "strategy": "dvm",
      "output_file": "app2_dvm"
    },
    {
      "package": "app3",
      "strategy": "dvm",
      "output_file": "app3_dvm"
    },
    {
      "package": "app3_alias",
      "strategy": "dvm",
      "output_file": "app3_alias_dvm"
    },
    {
      "package": "app1",
      "strategy": "native",
      "output_file": "app1",
      "linking_options": "build/lib_a.a build/lib_c.a build/lib_b.a"
    },
    {
      "package": "app2",
      "strategy": "native",
      "output_file": "app2",
      "linking_options": {
        "additional_linking_options": "build/lib_a.a",
        "link_c_standard_library": true
      }
    },
    {
      "package": "app3",
      "strategy": "native",
      "output_file": "app3",
      "linking_options": {
        "additional_linking_options": "build/lib_a.a build/lib_b.a build/lib_c.a",
        "link_c_standard_library": true
      }
    },
    {
      "package": "app3_alias",
      "strategy": "native",
      "output_file": "app3_alias",
      "linking_options": {
        "additional_linking_options": "build/lib_a.a build/lib_b.a build/lib_c.a",
        "link_c_standard_library": true
      }
    }
  ]
}"#
        );
    }

    fn packages_from_piotrek() -> Vec<Package> {
        vec![
            Package {
                id: "lib_a".into(),
                import_name: "lib_a".into(),
                version: Version::new(0, 1, 0),
                features: vec!["use_mathlib".into()],
                path_to_the_src_directory: PathBuf::from("duck_modules/multi_package/lib_a"),
                dependencies: vec![],
            },
            Package {
                id: "lib_b".into(),
                import_name: "lib_b".into(),
                version: Version::new(0, 1, 0),
                features: vec!["use_mathlib".into(), "use_lib_a".into()],
                path_to_the_src_directory: PathBuf::from("duck_modules/multi_package/lib_b"),
                dependencies: vec![
                    Dependency {
                        id: "lib_a".into(),
                        alias: Some("lib_a".into()),
                    },
                    Dependency {
                        id: "lib_c".into(),
                        alias: None,
                    },
                ],
            },
            Package {
                id: "lib_c".into(),
                import_name: "lib_c".into(),
                version: Version::new(0, 1, 0),
                features: vec!["use_lib_b".into()],
                path_to_the_src_directory: PathBuf::from("duck_modules/multi_package/lib_c"),
                dependencies: vec![Dependency {
                    id: "lib_b".into(),
                    alias: None,
                }],
            },
            Package {
                id: "app1".into(),
                import_name: "app1".into(),
                version: Version::new(0, 1, 0),
                features: vec![],
                path_to_the_src_directory: PathBuf::from("duck_modules/multi_package/app1"),
                dependencies: vec![
                    Dependency {
                        id: "lib_a".into(),
                        alias: Some("lib_a".into()),
                    },
                    Dependency {
                        id: "lib_c".into(),
                        alias: Some("lib_b".into()),
                    },
                ],
            },
            Package {
                id: "app2".into(),
                import_name: "app2".into(),
                version: Version::new(0, 1, 0),
                features: vec![],
                path_to_the_src_directory: PathBuf::from("duck_modules/multi_package/app2"),
                dependencies: vec![Dependency {
                    id: "lib_a".into(),
                    alias: Some("lib_a".into()),
                }],
            },
            Package {
                id: "app3".into(),
                import_name: "app3".into(),
                version: Version::new(0, 1, 0),
                features: vec![],
                path_to_the_src_directory: PathBuf::from("duck_modules/multi_package/app3"),
                dependencies: vec![
                    Dependency {
                        id: "lib_a".into(),
                        alias: None,
                    },
                    Dependency {
                        id: "lib_b".into(),
                        alias: None,
                    },
                ],
            },
            Package {
                id: "app3_alias".into(),
                import_name: "app3_alias".into(),
                version: Version::new(0, 1, 0),
                features: vec![],
                path_to_the_src_directory: PathBuf::from("duck_modules/multi_package/app3"),
                dependencies: vec![
                    Dependency {
                        id: "lib_a".into(),
                        alias: Some("lib_b".into()),
                    },
                    Dependency {
                        id: "lib_b".into(),
                        alias: Some("lib_a".into()),
                    },
                ],
            },
        ]
    }

    fn tasks_from_piotrek() -> Vec<Task> {
        vec![
            PackageCompilationTask {
                package_id: "lib_a".into(),
                strategy: PackageCompilationStrategy::Lib {
                    output_file: PathBuf::from("lib_a"),
                    archive_options: None,
                },
            },
            PackageCompilationTask {
                package_id: "lib_b".into(),
                strategy: PackageCompilationStrategy::Lib {
                    output_file: PathBuf::from("lib_b"),
                    archive_options: None,
                },
            },
            PackageCompilationTask {
                package_id: "lib_c".into(),
                strategy: PackageCompilationStrategy::Lib {
                    output_file: PathBuf::from("lib_c"),
                    archive_options: None,
                },
            },
            PackageCompilationTask {
                package_id: "lib_a".into(),
                strategy: PackageCompilationStrategy::Dvm {
                    output_file: PathBuf::from("lib_a_dvm"),
                },
            },
            PackageCompilationTask {
                package_id: "lib_b".into(),
                strategy: PackageCompilationStrategy::Dvm {
                    output_file: PathBuf::from("lib_b_dvm"),
                },
            },
            PackageCompilationTask {
                package_id: "lib_c".into(),
                strategy: PackageCompilationStrategy::Dvm {
                    output_file: PathBuf::from("lib_c_dvm"),
                },
            },
            PackageCompilationTask {
                package_id: "app1".into(),
                strategy: PackageCompilationStrategy::Dvm {
                    output_file: PathBuf::from("app1_dvm"),
                },
            },
            PackageCompilationTask {
                package_id: "app2".into(),
                strategy: PackageCompilationStrategy::Dvm {
                    output_file: PathBuf::from("app2_dvm"),
                },
            },
            PackageCompilationTask {
                package_id: "app3".into(),
                strategy: PackageCompilationStrategy::Dvm {
                    output_file: PathBuf::from("app3_dvm"),
                },
            },
            PackageCompilationTask {
                package_id: "app3_alias".into(),
                strategy: PackageCompilationStrategy::Dvm {
                    output_file: PathBuf::from("app3_alias_dvm"),
                },
            },
            PackageCompilationTask {
                package_id: "app1".into(),
                strategy: PackageCompilationStrategy::Binary {
                    output_file: PathBuf::from("app1"),
                    linking_options: Some(LinkerOptions::RawLinkerArgs(
                        "build/lib_a.a build/lib_c.a build/lib_b.a".into(),
                    )),
                },
            },
            PackageCompilationTask {
                package_id: "app2".into(),
                strategy: PackageCompilationStrategy::Binary {
                    output_file: PathBuf::from("app2"),
                    linking_options: Some(LinkerOptions::Complex(ComplexLinkerOptions {
                        linker: None,
                        additional_linking_options: Some("build/lib_a.a".into()),
                        link_cstd: Some(true),
                    })),
                },
            },
            PackageCompilationTask {
                package_id: "app3".into(),
                strategy: PackageCompilationStrategy::Binary {
                    output_file: PathBuf::from("app3"),
                    linking_options: Some(LinkerOptions::Complex(ComplexLinkerOptions {
                        linker: None,
                        additional_linking_options: Some(
                            "build/lib_a.a build/lib_b.a build/lib_c.a".into(),
                        ),
                        link_cstd: Some(true),
                    })),
                },
            },
            PackageCompilationTask {
                package_id: "app3_alias".into(),
                strategy: PackageCompilationStrategy::Binary {
                    output_file: PathBuf::from("app3_alias"),
                    linking_options: Some(LinkerOptions::Complex(ComplexLinkerOptions {
                        linker: None,
                        additional_linking_options: Some(
                            "build/lib_a.a build/lib_b.a build/lib_c.a".into(),
                        ),
                        link_cstd: Some(true),
                    })),
                },
            },
        ]
    }

    #[test]
    fn one_package() {
        let multi = MultiPackage {
            packages: vec![Package {
                id: "root".into(),
                import_name: "root".into(),
                version: Version::new(1, 0, 0),
                features: vec![],
                path_to_the_src_directory: PathBuf::from("root"),
                dependencies: vec![],
            }],
            tasks: vec![],
        };
        let serialized = serde_json::to_string_pretty(&multi).unwrap();
        assert_eq!(
            serialized,
            r#"{
  "packages": [
    {
      "id": "root",
      "name": "root",
      "version": "1.0.0",
      "features": [],
      "path": "root",
      "dependencies": []
    }
  ],
  "tasks": []
}"#
        )
    }

    #[test]
    fn one_package_one_task() {
        let multi = MultiPackage {
            packages: vec![Package {
                id: "root".into(),
                import_name: "root".into(),
                version: Version::new(1, 0, 0),
                features: vec![],
                path_to_the_src_directory: PathBuf::from("root"),
                dependencies: vec![],
            }],
            tasks: vec![PackageCompilationTask {
                package_id: "root".into(),
                strategy: PackageCompilationStrategy::EmitLLVMObject {},
            }],
        };
        let serialized = serde_json::to_string_pretty(&multi).unwrap();
        assert_eq!(
            serialized,
            r#"{
  "packages": [
    {
      "id": "root",
      "name": "root",
      "version": "1.0.0",
      "features": [],
      "path": "root",
      "dependencies": []
    }
  ],
  "tasks": [
    {
      "package": "root",
      "strategy": "obj"
    }
  ]
}"#
        )
    }
}
