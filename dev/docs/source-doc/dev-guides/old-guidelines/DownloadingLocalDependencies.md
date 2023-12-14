# Downloading local dependencies

We are currently using three external libraries: asio, crow, result.
They are set up as git-submodules and can be installed by:
~~~shell
$ git submodule update --init
~~~

~~~shell
$ cmake optional_flags
$ make # or `make target`
~~~
