# Tests

## Running tests

~~~shell
$ make build_test # compiles tests
$ make test # run tests, alternatively `ctest`
$ make memcheck_test # run tests under valgrind
~~~

## Testing coverage

~~~shell
$ cmake -DENABLE_COVERAGE=true .
$ make -j
$ make test
$ make coverage
$ xdg-open coverage/index.html
~~~
