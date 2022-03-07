Finit Test Suite
================

Finit comes with a set of tests and a small framework for running them.
Contributors are encouraged to write new tests when implementing features, or
fixing bugs.

Each test is run in isolation, in it's own namespace. Finit will therefore
be able to be launched as PID 1. Since it's also running with it's own root
directory it will be able to function properly without having any super user
privileges in the host environment.


Running tests
-------------

To run the test suite, first [build](../doc/build.md) Finit, e.g:

    ./configure --prefix=/usr --exec-prefix= --sysconfdir=/etc --localstatedir=/var
    make -j9 clean all

Then run (parallel does not work atm):

    make check

`make check` will set up the required assets for the test environment, and then
run the full set of tests. The environment is not removed afterwards so at
this point individual tests can be executed without having to run the entire
test suite, which is handy when developing new tests or debugging existing
test. To execute an individual test, simply invoke the script containing it:

    ./test/name-of-the-test.sh

Another way to run a single (or more) test(s) is to define the `TESTS`
environment variable:

    TESTS="start-kill-service" make check
