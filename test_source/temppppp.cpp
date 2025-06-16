#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <filesystem>
#include <vector>

class BIT {
    public :

        /*
          초기 부팅 시 장치 상태 감시 (polling)
          장치 연결 확인되면 넘어감
        */
        void pbit();
        /**
            tcp 연결 되었을 때 동작함. 만약 장치 상태가 이상하다면 장치상태 업데이트 
            및 시스템 상태 업데이트
        */
        void cbit();
        /* 캠 확인*/
        bool isCamConnected();
        /*  TPU 확인*/
        bool isTpuConnected();
        /*  온도 체크*/
        double getTemp();

    private:
    std::string readFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return "";
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    std::string trim(const std::string& s) {
        auto start = s.find_first_not_of(" \t\n\r");
        auto end = s.find_last_not_of(" \t\n\r");
        return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    }

    int parseInt(const std::string& s) {
        return std::stoi(trim(s));
    }

    double parseDouble(const std::string& s) {
        return std::stod(trim(s));
    }
};
int main() {
    BIT bit;
    double asdf;
    asdf=bit.getTemp();
    std::cout<< asdf<<std::endl;
}
void BIT::pbit() {
    std::cout << "start power BIT" << std::endl;


}

void BIT::cbit() {

   std::cout << "start continous BIT" << std::endl;



}


bool BIT::isCamConnected() {
    return true;
} 

bool BIT::isTpuConnected() {
    /**
        TODO
    */
    return true;
}



double BIT::getTemp() {
    const std::string dev = "/sys/bus/iio/devices/iio:device0";

    int raw = parseInt(readFile(dev + "/in_temp0_raw"));
    int offset = parseInt(readFile(dev + "/in_temp0_offset"));
    double scale = parseDouble(readFile(dev + "/in_temp0_scale"));

    double mdeg = (raw + offset) * scale;   // milli-degree Celsius
    return mdeg / 1000.0;                   // degree Celsius

}