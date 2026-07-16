Duckling scripts in QuackPack
=============================
Introduction
------------
QuackPack classifies Duckling scripts into three groups:

1. Scripts inside packages (PackageLoader can find a package starting from the script's path).
They are viewed as part of the package, share its venv (can use the same dependencies, are run with the same profiles etc.) and cannot have frontmatters.

2. Standalone scripts without frontmatters.
Those are treated as scripts of type 1, but with the global venv instead of any package.

3. Scripts with frontmatters.
Those are truly standalone and have their own temporary venvs.

Build artifacts
---------------
1. For scripts of the first type, the compilation artifacts of the script lie in the build artifacts of the package.

2. This is also true for scripts of type 2. Their artifacts lie in the global venv's build artifact, which itself lies inside the duck home.

3. For scripts of the third type we create `.duck_build` folder inside the folder in which the script is located.
The precise artifacts directory is `<path to the folder of the script>/.duck_build/<name of the script>`.

Thus all the differences are abstracted away before compilation, which can be then performed homogenously.

Handling
--------
1.  For all types of scripts we first get their venvs.
For scripts of type 1 & 2 it is the venv of the package the script is located in / the global venv.
Thus we construct `AnyPackage::Package`.
Scripts with frontmatters have their own venvs, thus we construct `AnyPackage::Venv`.

2. Then the process is homogenous, we synchronize the venvs, compile all the venv's dependencies and then compile the script.