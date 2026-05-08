//! Schema of the multipackage JSON sent to duckc from QuackPack
//!
//! There are a few nuances we have to remember about:
//! * when compiling a package, we need to pass its __entire__ subtree to duckc,

use std::path::PathBuf;

use serde::{Deserialize, Serialize, de, ser};

use crate::StrId;
use crate::quackpack::core::{FeatureName, Version};

#[derive(Default, Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
pub struct DuckcMultiPackage {
    /// Packages required for a successful duckc invocation; packages have to contain their __entire__ dependencies trees.
    pub packages: Vec<DuckcPackage>,
    /// Duckc tasks to finish in this invocation.
    pub tasks: Vec<DuckcTask>,
}

#[derive(Default, Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
pub struct DuckcPackage {
    #[serde(rename = "name")]
    /// ID of a package to compile. Note, that it doesn't have to be a package's name, it can be a
    /// unique id.
    /// Has to be unique in terms of the entire [`packages`](DuckcMultiPackage::packages) vector.
    pub id: StrId,
    /// Version of a package we're currently compiling.
    pub version: Version,
    /// Enabled features for this package.
    pub features: Vec<FeatureName>,
    #[serde(rename = "path")]
    /// Path to the source directory of this package.
    pub path_to_the_src_directory: PathBuf,
    /// Dependencies of this package.
    pub dependencies: Vec<DuckcDependency>,
}

#[derive(Default, Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
/// A single package's dependency, in a duckc-friendly format.
pub struct DuckcDependency {
    #[serde(rename = "name")]
    /// ID of a package. Note, that there must a package with `id = self.id` in a [`packages`](DuckcMultiPackage::packages) vector.
    pub id: StrId,
    #[serde(rename = "alias")]
    /// How should this package be named, when resolving imports.
    pub import_name: StrId,
}

// `compiler/driver/driver/src/driver/task/task.cpp` deserializes `RawTask` as `RawPackageCompilationTask`.
// So, from JSON POV, they are the same type.
pub type DuckcTask = DuckcPackageCompilationTask;

#[derive(Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
pub struct DuckcPackageCompilationTask {
    #[serde(rename = "package")]
    /// ID of a package refered by this task.
    pub package_id: StrId,
    #[serde(flatten)]
    /// Compilation strategy of this task.
    pub strategy: DuckcPackageCompilationStrategy,
}

#[derive(Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
// These and the flatten above are required for generating valid JSON's, while still giving us type safety :^).
#[serde(rename_all = "snake_case", tag = "strategy")]
/// Supported duckc compilation strategies.
pub enum DuckcPackageCompilationStrategy {
    /// Compile this task into a DVM file.
    Dvm { // Name has to be `Dvm`, as `DVM` (with "snake_case") would be rendered as "d_v_m".
        /// Set an explicit output filename. By default `package_dvm` is used.
        #[serde(skip_serializing_if = "Option::is_none")]
        output_file: Option<PathBuf>,
    },
    Native {
        /// Path to the output file.
        output_file: PathBuf,
        /// Additional linking options.
        #[serde(skip_serializing_if = "Option::is_none")]
        linking_options: Option<DuckcLinkingOptions>,
    },
    Lib {
        /// Path to the output file.
        output_file: PathBuf,
        /// Additional archiving options.
        #[serde(skip_serializing_if = "Option::is_none")]
        archive_options: Option<DuckcArchiveOptions>,
    },
}

#[derive(Debug, Clone, Eq, PartialEq, Hash)]
/// Possible variants of linking options passed to duckc.
pub enum DuckcLinkingOptions {
    /// Add extra linker arguments.
    RawLinkerArgs(String),
    Complex(ComplexLinkingOptions),
}

impl ser::Serialize for DuckcLinkingOptions {
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

impl<'de> de::Deserialize<'de> for DuckcLinkingOptions {
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
pub struct ComplexLinkingOptions {
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
pub enum DuckcArchiveOptions {
    /// Set an explicit archiver path.
    Archiver(PathBuf),
    Complex(ComplexArchiveOptions),
}

impl ser::Serialize for DuckcArchiveOptions {
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

impl<'de> de::Deserialize<'de> for DuckcArchiveOptions {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        serde_untagged::UntaggedEnumVisitor::new()
            .expecting("a valid duckc linking options")
            .string(|string| Ok(Self::Archiver(string.into())))
            .map(|map| map.deserialize().map(Self::Complex))
            .deserialize(deserializer)
    }
}

#[derive(Default, Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
/// More complex archiver options.
pub struct ComplexArchiveOptions {
    /// Set an explicit archiver.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub archiver: Option<PathBuf>,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn basic_json_convertion() {
        let mut multi_package = DuckcMultiPackage::default();
        populate_packages(&mut multi_package.packages);
        populate_tasks(&mut multi_package.tasks);
        let serialized = serde_json::to_string_pretty(&multi_package).unwrap();
        assert_eq!(
            serialized,
            r#"{
  "packages": [
    {
      "name": "a",
      "version": "1.0.0",
      "features": [],
      "path": "",
      "dependencies": [
        {
          "name": "dep-hash",
          "alias": "bar"
        },
        {
          "name": "dep2-hash",
          "alias": "foo"
        }
      ]
    },
    {
      "name": "dep-hash",
      "version": "2.0.0",
      "features": [
        "feature"
      ],
      "path": "",
      "dependencies": [
        {
          "name": "dep2-hash",
          "alias": "foo-aliased"
        }
      ]
    },
    {
      "name": "dep2-hash",
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
      "package": "dep-hash",
      "strategy": "dvm"
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

        let deserialized = serde_json::from_str::<DuckcMultiPackage>(&serialized).unwrap();
        assert_eq!(deserialized, multi_package);
    }

    fn populate_packages(packages: &mut Vec<DuckcPackage>) {
        packages.push(DuckcPackage {
            id: "a".into(),
            version: Version::new(1, 0, 0),
            features: vec![],
            path_to_the_src_directory: PathBuf::default(),
            dependencies: vec![
                DuckcDependency {
                    id: "dep-hash".into(),
                    import_name: "bar".into(),
                },
                DuckcDependency {
                    id: "dep2-hash".into(),
                    import_name: "foo".into(),
                },
            ],
        });
        packages.push(DuckcPackage {
            id: "dep-hash".into(),
            version: Version::new(2, 0, 0),
            features: vec!["feature".into()],
            path_to_the_src_directory: PathBuf::default(),
            dependencies: vec![DuckcDependency {
                id: "dep2-hash".into(),
                import_name: "foo-aliased".into(),
            }],
        });
        packages.push(DuckcPackage {
            id: "dep2-hash".into(),
            version: Version::new(2, 1, 37),
            features: vec!["foo".into(), "bar".into()],
            path_to_the_src_directory: PathBuf::default(),
            dependencies: vec![],
        });
    }
    fn populate_tasks(tasks: &mut Vec<DuckcTask>) {
        tasks.push(DuckcPackageCompilationTask {
            package_id: "a".into(),
            strategy: DuckcPackageCompilationStrategy::Native {
                output_file: PathBuf::from("out.exe"),
                linking_options: Some(DuckcLinkingOptions::Complex(ComplexLinkingOptions {
                    linker: None,
                    additional_linking_options: Some("-lfoo".into()),
                    link_cstd: Some(true),
                })),
            },
        });
        tasks.push(DuckcPackageCompilationTask {
            package_id: "a".into(),
            strategy: DuckcPackageCompilationStrategy::Native {
                output_file: PathBuf::from("out.exe"),
                linking_options: Some(DuckcLinkingOptions::RawLinkerArgs("-lfoo".into())),
            },
        });
        tasks.push(DuckcPackageCompilationTask {
            package_id: "a".into(),
            strategy: DuckcPackageCompilationStrategy::Native {
                output_file: PathBuf::from("out.exe"),
                linking_options: None,
            },
        });
        tasks.push(DuckcPackageCompilationTask {
            package_id: "dep-hash".into(),
            strategy: DuckcPackageCompilationStrategy::Dvm {
                output_file: Some(PathBuf::from("out.dvm")),
            },
        });

        tasks.push(DuckcPackageCompilationTask {
            package_id: "dep-hash".into(),
            strategy: DuckcPackageCompilationStrategy::Dvm { output_file: None },
        });

        tasks.push(DuckcPackageCompilationTask {
            package_id: "dep2-hash".into(),
            strategy: DuckcPackageCompilationStrategy::Lib {
                output_file: PathBuf::from("out.so"),
                archive_options: Some(DuckcArchiveOptions::Archiver("ar".into())),
            },
        });

        tasks.push(DuckcPackageCompilationTask {
            package_id: "dep2-hash".into(),
            strategy: DuckcPackageCompilationStrategy::Lib {
                output_file: PathBuf::from("out.so"),
                archive_options: Some(DuckcArchiveOptions::Complex(ComplexArchiveOptions {
                    archiver: Some("ar".into()),
                })),
            },
        });

        tasks.push(DuckcPackageCompilationTask {
            package_id: "dep2-hash".into(),
            strategy: DuckcPackageCompilationStrategy::Lib {
                output_file: PathBuf::from("out.so"),
                archive_options: Some(DuckcArchiveOptions::Complex(ComplexArchiveOptions {
                    archiver: None,
                })),
            },
        });

        tasks.push(DuckcPackageCompilationTask {
            package_id: "dep2-hash".into(),
            strategy: DuckcPackageCompilationStrategy::Lib {
                output_file: PathBuf::from("out.so"),
                archive_options: None,
            },
        });
    }
}
