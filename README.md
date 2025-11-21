# Kernel Taint Tracking Prototype

This repository is a fork of the original [llvm-project](https://github.com/llvm/llvm-project) with an out-of-tree compiler pass designed to perform taint analysis on Linux kernel modules.

## Recommended environment for testing

Any x86-64 Linux distribution. The [ninja](https://ninja-build.org/) build system is required for building LLVM as described below. 

## Testing with `opt` (simplest option)

For testing purposes, you can use the ```opt``` utility in the LLVM toolchain. First, build the compiler pass out-of-tree by running the following commands from the root of the repository:

```shell
cd kernel-taint-tracker

cmake -S . ./build

ninja -C build 

```

After the build is successful, run the following command to execute the compiler pass on the `.ll` file specified by `<filename>`:

```shell
opt -load-pass-plugin ./build/TaintTrackerPass.so -passes=taint-analysis -disable-output <filename>
```

#### Note:
By default, `opt` outputs the module bitcode of the LLVM modules specified by `<filename>`. This is useful for optimization passes that modify the given module. Since this compiler pass only performs analysis, the module remains unchanged. If you want to see the output of the module under test, remove the flag `-disable-output` and add the flag `-S` to the command to print the LLVM intermediate representation of the module.

For more information on how `opt` operates, check out the [LLVM documentation](https://llvm.org/docs/CommandGuide/opt.html).

## Building the LLVM with the compiler pass integrated

For setting up the build pipeline for LLVM and the necessary projects, run the following command:

```shell
cmake -S llvm -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DLLVM_TARGETS_TO_BUILD="X86" \
  -DLLVM_ENABLE_PROJECTS="clang" \
  -DLLVM_INCLUDE_TESTS=OFF \
  -DLLVM_INCLUDE_EXAMPLES=OFF \
  -DLLVM_INCLUDE_DOCS=OFF \
  -DLLVM_BUILD_TOOLS=ON \
  -DLLVM_PARALLEL_LINK_JOBS=1 \
  -DLLVM_USE_LINKER=lld
```
After the build files are successfully generated, you can build LLVM and Clang using:

```shell
ninja -C build
```

#### Note:
Depending on your machine, building all the necessary LLVM projects from source may take several hours!
