@echo off
REM Builds server.exe from the C++ files in backend\  (run it from the project folder)
REM Edit MYSQL below if MySQL is installed somewhere else.
REM libmysql.dll and libwinpthread-1.dll must be in this folder to RUN server.exe.
set MYSQL=C:\Program Files\MySQL\MySQL Server 8.0

g++ -std=c++17 -Wall -Wextra -static-libgcc -static-libstdc++ -o server.exe ^
  backend\main.cpp backend\app.cpp ^
  backend\core\*.cpp backend\db\*.cpp backend\services\*.cpp backend\handlers\*.cpp ^
  -I"%MYSQL%\include" -L"%MYSQL%\lib" -lmysql -lws2_32 -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic

if errorlevel 1 (
  echo.
  echo BUILD FAILED
  exit /b 1
)
echo Build OK: server.exe
