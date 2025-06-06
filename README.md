## EOIR_Simulator 프로젝트
#### C# 7.3
#### .NET Framework 4.7.2
#### 플랫폼 대상 : x64
#### 패키지 : 도구 - NuGet 패키지 관리자 - 솔루션용 NuGet 패키지 관리 - HelixToolKit.Wpf 2.12.0 설치
#### C:\workspace_WPF\EOIR_Simulator\EOIR_Simulator\bin\Debug 에 FrameSender.dll(C:\workspace_nex1_C++\FrameSender\x64\Debug\FrameSender.dll) 넣어줘야함


## FrameSender 프로젝트 속성

### C/C++ - 일반 - 추가 포함 디렉터리
#### C:\opencv\build\include
#### C:\gstreamer\1.0\msvc_x86_64\include\gstreamer-1.0
#### C:\gstreamer\1.0\msvc_x86_64\include\glib-2.0
#### C:\gstreamer\1.0\msvc_x86_64\include\glib-2.0\glib
#### C:\gstreamer\1.0\msvc_x86_64\lib\glib-2.0\include

### 링커 - 일반 - 추가 라이브러리 디렉터리
#### C:\gstreamer\1.0\msvc_x86_64\lib
#### C:\opencv\build\x64\vc16\lib

### 링커 - 입력 - 추가 종속성
#### gobject-2.0.lib
#### glib-2.0.lib
#### gstreamer-1.0.lib
#### opencv_world4100.lib
#### opencv_world4100d.lib
#### gstapp-1.0.lib
#### gstbase-1.0.lib
#### gstvideo-1.0.lib


## receiver 프로젝트 속성

### C/C++ - 일반 - 추가 포함 디렉터리
#### C:\gstreamer\1.0\msvc_x86_64\include\gstreamer-1.0
#### C:\gstreamer\1.0\msvc_x86_64\include\glib-2.0
#### C:\gstreamer\1.0\msvc_x86_64\include\glib-2.0\glib
#### C:\gstreamer\1.0\msvc_x86_64\lib\glib-2.0\include

### 링커 - 일반 - 추가 라이브러리 디렉터리
#### C:\gstreamer\1.0\msvc_x86_64\lib

### 링커 - 입력 - 추가 종속성
#### gstreamer-1.0.lib
#### gstbase-1.0.lib
#### gstrtp-1.0.lib
#### gstapp-1.0.lib
#### gstvideo-1.0.lib
#### gobject-2.0.lib
#### glib-2.0.lib
#### gstpbutils-1.0.lib

### Window Gstreamer 1.0
### OpenCV 4.10.0
