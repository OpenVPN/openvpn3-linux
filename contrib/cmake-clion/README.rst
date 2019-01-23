IDE support - CMake
===================

Some IDE suites, like CLion, work better when having a CMake build chain
configured.  CMake is NOT recommended for production builds and is
**unsupported** by the OpenVPN 3 Linux project.  Using this approach may
or may not work for you, YMMV.  It is **not** a project goal to ensure this
CMake setup will always be up-to-date or work.  If this turns out to cause
too much noise in future commits, this will be deprecated and removed.

The CMakeLists.txt file here can be put into the project root directory and
should enable the extended features provided by IDE environments supporting
this.

