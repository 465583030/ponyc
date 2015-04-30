# Getting help 

* [Open an issue!](https://github.com/CausalityLtd/ponyc/issues)

* Use the [mailing list](mailto:developers@causality.io).

* A tutorial is available [here](http://tutorial.ponylang.org).

# Editor support

* Sublime Text: [Pony Language](https://packagecontrol.io/packages/Pony%20Language)

* Atom: [language-pony](https://atom.io/packages/language-pony)

* Visual Studio: pending.

* Vim: pending.

# Building on Linux [![build status](http://ponylang.org:50000/buildStatus/icon?job=ponyc)](http://ci.ponylang.org/job/ponyc/)

First, install LLVM 3.6 using your package manager. You may need to install zlib and ncurses as well.

This will build ponyc and compile helloworld:

```
$ make config=release
$ ./build/release/ponyc examples/helloworld
```

To build a NUMA-aware runtime, install libnuma-dev using your package manager and build as follows:

```
$ make use=numa config=release
```

# Building on Mac OSX

First, install [homebrew](http://brew.sh) if you haven't already. Then, brew llvm36, like this:

```
$ brew tap homebrew/versions
$ brew install llvm36
```

This will build ponyc and compile helloworld:

```
$ make config=release
$ ./build/release/ponyc examples/helloworld
```

# Building on Windows

The LLVM 3.6 prebuilt binaries for Windows do NOT include the LLVM development tools and libraries. Instead, you will have to build and install LLVM 3.6 from source. You will need to make sure that the path to LLVM/bin (location of llvm-config) is in your PATH variable.

You will also need to build and install premake5 (not premake4) from source. We need premake5 in order to support current versions of Visual Studio.

You may also need to install zlib and ncurses.

```
$ premake5 vs2013
$ Release build with Visual Studio (ponyc.sln)
$ ./build/release/ponyc examples/helloworld
```

