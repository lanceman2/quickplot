# quickplot

quickplot is an interactive 2D plotter.  You can very quickly swim through
data with quickplot.  quickplot is GPL(v3) free software.

This source package contains HTML files, man pages and text files that
document quickplot. The [quickplot home
page](http://quickplot.sourceforge.net/) contains the latest quickplot
documentation.

## Building quickplot from tarball source

quickplot is a standard GNU Autotools built package
run:

```bash
  ./configure
  make
  make install
```

or run:

```bash
  ./configure --help
```

for a long list of configuration options.  See instructions below for how
to build and install from git repository source code.

See http://quickplot.sourceforge.net/about.html for
Installation prerequisites and other details.


## Building quickplot from GitHub repository source

If you are building quickplot from the GitHub repository source files,
when you run configure you must use the ``--enable-developer`` option
to enable the building of addition files that are in tarball
distributions, but not in the git repository.  For example run:

```bash
  ./bootstrap
  ./configure --prefix /home/joe/installed/encap/quickplot \
    --enable-developer  --enable-debug
  make -j5
  make install
```


## Development and Testing Package

There is another optional quickplot package for development and testing
the quickplot source code.  Test/junk programs in this package do things
like: display the default quickplot point and line colors in one window,
or stupid things like read and print "NAN" as a double; just so
developers know.  You can get this "quickplot_tests" package by:

```bash
  svn co\
    https://quickplot.svn.sf.net/svnroot/quickplot/quickplot_tests\
    quickplot_tests
```

Running the above line will make the directory quickplot_tests in the
current directory.


## Features to come

- Add configure option --enable-developer=auto to be the default.
- Add middle mouse wheel zooming, like in google maps.
