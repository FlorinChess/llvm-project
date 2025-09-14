# Kernel Taint Tracking Prototype

This repository is a fork of the original [llvm-project](https://github.com/llvm/llvm-project) with an out-of-tree compiler pass designed to perform taint analysis on Linux kernel modules.

## Recommended environment for testing

Any x86-64 Linux distribution.

## How to build

Building the compiler pass requires the build system [ninja](https://ninja-build.org/) to be installed on your machine. With ninja installed, run

```shell
ninja -C build
```

in the directory `kernel-taint-tracker` to build the compiler pass.
