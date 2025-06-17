# build
```
source /workspace/peta_prj/Clocked_pwm_peta_v2/zynq_sdk/environment-setup-cortexa9t2hf-neon-xilinx-linux-gnueabi
export TOOLCHAIN_DIR=/opt/gcc-linaro-12.2.1-2022.11-x86_64_arm-linux-gnueabihf
export CC=${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-gcc
export CXX=${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-g++

cd src
mkdir build && cd build

cmake .. \
  -DCMAKE_SYSROOT=${SDKTARGETSYSROOT} \
  -DCMAKE_FIND_ROOT_PATH=${SDKTARGETSYSROOT} \
  -DTARGET_CPU=cortex-a9 \
  -DTARGET_FPU=neon \
  -DTARGET_FLOAT=hard   \
  -DCMAKE_BUILD_TYPE=Release

make
```

# run
```
sudo ./detection --model <model_name> --labels <label_name>
```

# fps
9~10 fps