# Duckling scripts in QuackPack

## Introduction

QuackPack classifies Duckling scripts into two groups:

### Scripts inside packages ([`PackageLoader`](package_loader.rs) can find a package starting from the script's path).

They are viewed as part of the package, share its venv (can use the same dependencies, are run with the same profiles etc.) and cannot have frontmatters.

### Standalone scripts.

They can (but do not have to) contain a frontmatter.
From the parsing perspective, missing frontmatter == empty frontmatter.

> [!IMPORTANT]
> By default [`PackageLoader`](package_loader.rs) treats scripts without frontmatters as scripts under the global package (the type above).

They have temporary venvs.

## Build artifacts

1. For scripts of the first type, the compilation artifacts of the script lie in the build artifacts of the package.

2. For scripts of the second type we create `.duck_build` folder inside the folder in which the script is located.
The precise artifacts directory is `<path to the folder of the script>/.duck_build/<name of the script>`.

Thus all the differences are abstracted away before compilation, which can be then performed homogeneously.

## Handling

1.  For all types of scripts we first get their venvs.
For scripts of type 1 it is the venv of the package the script is located in / the global venv.
Thus we construct `AnyPackage::Script(Script::Associated(..))`.
Standalone scripts have their own venvs, thus we construct `AnyPackage::Script(Script::Standalone(..))`.

2. Then the process is homogeneous, we synchronize the venv, compile all the venv's dependencies and then compile the script.
