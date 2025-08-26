clang -emit-llvm -S ~/BACPROJECT/Kernel-Module/linux_kernel_module.c -o ./llvm-testcases/linux_kernel_module.ll \
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

