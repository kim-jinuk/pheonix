# toolchain.cmake
# 
set(CMAKE_C_COMPILER "/home/dan265/myproject0520/sysroots/x86_64-petalinux-linux/usr/bin/arm-xilinx-linux-gnueabi/arm-xilinx-linux-gnueabi-gcc")
set(CMAKE_CXX_COMPILER "/home/dan265/myproject0520/sysroots/x86_64-petalinux-linux/usr/bin/arm-xilinx-linux-gnueabi/arm-xilinx-linux-gnueabi-g++")

# 아래 줄 필수: SDK에서 자동 설정된 전체 플래그 복사
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -fstack-protector-strong -O2 -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security -Werror=format-security")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -fstack-protector-strong -O2 -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security -Werror=format-security")

# sysroot 명시
set(CMAKE_SYSROOT "/home/dan265/myproject0520/sysroots/cortexa9t2hf-neon-xilinx-linux-gnueabi")
set(CMAKE_FIND_ROOT_PATH )
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

