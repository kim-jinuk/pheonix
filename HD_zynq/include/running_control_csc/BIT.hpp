
#pragma once

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
};

