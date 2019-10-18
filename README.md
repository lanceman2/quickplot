# Quickplot

The Quickplot [home page](http://quickplot.sourceforge.net/) contains the lastest Quickplot
documentation. This Quickplot documentation page is also part of the Quickplot source.  
This README file is just a brief introduction to Quickplot.

## What is Quickplot?

Quickplot is an interactive 2D plotter.  You can very
quickly swim through data with Quickplot.

Quickplot is GPL(v3) free software.

The Quickplot source package installs HTML files, man pages
and text files that document Quickplot.

## Building QuickPlot

Quickplot is a standard GNU Autotools built package
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

for a long list of configuration options.

See http://quickplot.sourceforge.net/about.html for
Installation prerequisites and other details.

## Development and Testing Package

There is another optional Quickplot package for development and testing
the Quickplot source code.  Test/junk programs in this package do things
like: display the default Quickplot point and line colors in one window,
or stupid things like read and print "NAN" as a double; just so developers
know.  You can get this "quickplot_tests" package by:

```bash
svn co\
https://quickplot.svn.sf.net/svnroot/quickplot/quickplot_tests\
quickplot_tests
```

Running the above line will make the directory quickplot_tests in the
current directory.

If you are building Quickplot from the GitHub repository source files,
when you run configure you must use the --enable-developer option
to enable the building of addition files that are in tarball
distributions, but not in the git repository.  For example run:

```bash
./bootstrap
./configure --prefix /home/joe/installed/encap/quickplot \
  --enable-developer  --enable-debug
make -j5
make install
```

TODO: Add --enable-developer=auto to be the default
