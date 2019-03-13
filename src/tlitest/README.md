Tlog Integration Tests
======================

Tlog Integration Tests provide real world tests for various features
and functionalities of tlog.  Although these tests are meant for
OS package build gating, they can be run for general testing purposes
as well.

This README.md will provide information on how to prepare for, run,
and cleanup after the tests.

Setup
-----

In the src/tlitest directory, run the main setup script:
    ./tlitest-setup

This will install necessary packages, add users and setup a wheel
based no password sudoer rule file.

As a part of setup, we also take note of the specified packages that
were already installed before setup.  This allows us to skip them
during package removal of cleanup (teardown).

Also, during setup, we take note of users the same way so that
pre-existing users will skip delete during cleanup.

Run
---

To run the tests, you must execute pytest.  In the src/tlitest
directory run:
    ./tlitest-run

If you want to generate a junit xml file for upload for reporting,
run:
    ./tlitest-run --junit-xml=/tmp/path-to-your-junit-xml-file

Cleanup
-------

The cleanup after the tests, inside the src/tlitest directory run:
    ./tlitest-teardown

This will look for pre-existing packages and skip them.  Packages
installed by setup will be removed.

This will also look for pre-existing users and skip them. Users
added by setup will be removed.

The sudoers rule file added by setup will also be removed.
