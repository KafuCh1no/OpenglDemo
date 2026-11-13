@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1

set CMAKE="C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set NINJA="C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"

%CMAKE% -S D:\Demo -B D:\Demo\out\build\x64-Debug -G Ninja ^
  -DCMAKE_BUILD_TYPE:STRING=Debug ^
  -DCMAKE_INSTALL_PREFIX:PATH=D:\Demo\out\install\x64-Debug ^
  -DCMAKE_MAKE_PROGRAM=%NINJA% ^
  -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake
if errorlevel 1 exit /b 1

%CMAKE% --build D:\Demo\out\build\x64-Debug
if errorlevel 1 exit /b 1

echo BUILD_OK
