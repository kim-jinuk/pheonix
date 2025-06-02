#pragma once
#ifdef FRAMESENDER_EXPORTS
#define FRAMESENDER_API __declspec(dllexport)
#else
#define FRAMESENDER_API __declspec(dllimport)
#endif

extern "C" {
    FRAMESENDER_API void InitSender(const char* pipeline);
    FRAMESENDER_API void SendFrame(const unsigned char* jpegData, int length);
    FRAMESENDER_API void CloseSender();
}