@echo off
REM builds both programs: analyzer.exe (terminal menu) and server.exe (web version)
cd /d "%~dp0"
if exist C:\MinGW\bin set PATH=C:\MinGW\bin;%PATH%
where g++ >nul 2>nul || (echo g++ not found. Install MinGW and add its bin folder to PATH. & pause & exit /b 1)
echo Building analyzer.exe ...
g++ -std=c++11 -Wall src/main.cpp src/Network.cpp -o analyzer.exe -static || (pause & exit /b 1)
echo Building server.exe ...
g++ -std=c++11 -Wall src/server.cpp src/Network.cpp -o server.exe -lws2_32 -static || (pause & exit /b 1)
echo Done.
