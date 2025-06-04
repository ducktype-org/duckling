Workflow
========

Dependency solving is done in four consecutive steps:

1. Information gathering - in this stage an asynchronous DFS-like branching algorithm is performed to find all of the packages which possibly can be used in the solution.

2. Construction of the dependencies function - in this stage, where all of the necesseary configuration files have been already obtained, each package for which information was sucessfully obtained has its dependencies listed.

3. Information cleaning - a reversed topological sort-like algorithm is performed to find all of the packages which cannot be used. Such packages are all of the packages for which a configuration file couldn't be obtained or any package having a dependency such that any realisation of that dependency cannot be used.

4. Solver engine - at this stage, after all the necesseary information had been gathered, a ILP is constructed and fed into pyscipopt to find an optimised solution.