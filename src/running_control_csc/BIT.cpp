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
    sysInfo.TPU_state=isTpuConnected();


}


bool BIT::isCamConnected() {
    return access("/dev/video0", F_OK) == 0;
} 

bool BIT::isTpuConnected() {
    /**
        TODO
    */
    return true;
}



