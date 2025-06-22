#include "running_control_csc/BIT.hpp"
#include "running_control_csc/globals.hpp"

#include <iostream>
#include <unistd.h>     // access, sleep
#include <fcntl.h>      // F_OK
#include <thread>
#include <chrono>
#include <atomic>

void BIT::pbit() {
    std::cout << "start power BIT" << std::endl;

    while (!(isCamConnected() && isTpuConnected())) {
        std::cout << "running pbit ..." << std::endl;
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    sysInfo.CAM_state=true;
    sysInfo.TPU_state=true;
    std::cout << "complete power BIT" <<std::endl;
}

void BIT::cbit() {

   // std::cout << "start continous BIT" << std::endl;
    
    sysInfo.CAM_state=isCamConnected();
   // sysInfo.TPU_state=isTpuConnected();
    sysInfo.cpu_temp=getTemp();

}


bool BIT::isCamConnected() {
    return access("/dev/video0", F_OK) == 0;
} 

bool BIT::isTpuConnected() {
    FILE* pipe = popen("lsusb", "r");
    if (!pipe) return false;

    char buffer[128];
    bool found = false;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        if (strstr(buffer, "1a6e:089a") || strstr(buffer, "18d1:9302")) {
            found = true;
            break;
        }
    }
    pclose(pipe);
    return found;
}



float BIT::getTemp() {
                   // degree Celsius
    const std::string dev = "/sys/bus/iio/devices/iio:device0";

    int raw = parseInt(readFile(dev + "/in_temp0_raw"));
    int offset = parseInt(readFile(dev + "/in_temp0_offset"));
    float scale = static_cast<float>(parseDouble(readFile(dev + "/in_temp0_scale")));  // float로 캐스팅

    float mdeg = (raw + offset) * scale;   // milli-degree Celsius
    float temp = mdeg / 1000.0f;           // degree Celsius

  //  std::cout << "[Debug] CPU Temp: " << temp << " °C" << std::endl;

    return temp;

}