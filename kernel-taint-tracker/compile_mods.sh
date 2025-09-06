#!/bin/bash

OUT_DIR="./llvm-modules"
KERNEL_MODULES_DIR="./../../Kernel-Module"

# Create output directory if it doesn't exist
mkdir -p "$OUT_DIR"

# Loop through each .c file in the source directory
for src_file in "${KERNEL_MODULES_DIR}"/*.c; do
    
    # Get the filename without the path and extension
    filename=${src_file##*/}
    name_no_ext=${filename%.c}
    
    OUT_FILE="${name_no_ext}.ll"

    # Compile the file with clang
    clang -S -emit-llvm "${src_file}" -o "${OUT_DIR}"/"${OUT_FILE}" \
      -I/usr/src/linux-headers-$(uname -r)/arch/x86/include \
      -I/usr/src/linux-headers-$(uname -r)/arch/x86/include/generated \
      -I/usr/src/linux-headers-$(uname -r)/include \
      -I/usr/src/linux-headers-$(uname -r)/arch/x86/include/uapi \
      -I/usr/src/linux-headers-$(uname -r)/arch/x86/include/generated/uapi \
      -I/usr/src/linux-headers-$(uname -r)/include/uapi \
      -I/usr/src/linux-headers-$(uname -r)/include/generated/uapi \
      -I/usr/src/linux-headers-$(uname -r)/ubuntu/include \
      -D__KERNEL__ -DMODULE \
      -include /usr/src/linux-headers-$(uname -r)/include/linux/compiler-version.h \
      -include /usr/src/linux-headers-$(uname -r)/include/linux/kconfig.h \
      -include /usr/src/linux-headers-$(uname -r)/include/linux/compiler_types.h \
      -target x86_64-pc-linux-gnu \
      -fno-discard-value-names \
      -O0 -fno-rtti -fno-exceptions -fno-asynchronous-unwind-tables \
    
    # Check if compilation succeeded
    if [ $? -eq 0 ]; then
        echo "Compiled $src_file -> $OUT_DIR/$OUT_FILE"
    else
        echo "Failed to compile $src_file"
    fi
done

# clang -emit-llvm -S ~/BACPROJECT/Kernel-Module/linux_kernel_module.c -o ./llvm-testcases/linux_kernel_module.ll \
  

