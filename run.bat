@echo off
REM builds the server and opens the web version in the browser
cd /d "%~dp0"
if exist C:\MinGW\bin set PATH=C:\MinGW\bin;%PATH%
where g++ >nul 2>nul || (echo g++ not found. Install MinGW and add its bin folder to PATH. & pause & exit /b 1)
g++ -std=c++11 src/server.cpp src/Network.cpp -o server.exe -lws2_32 -static || (pause & exit /b 1)
start "" http://localhost:8080
server.exe
