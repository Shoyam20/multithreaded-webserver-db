@echo off
REM Builds server.exe from everything in backend\
REM Edit MYSQL below if MySQL is installed somewhere else.
set MYSQL=C:\Program Files\MySQL\MySQL Server 8.0

g++ -std=c++17 -Wall -Wextra -static-libgcc -static-libstdc++ -o server.exe ^
  backend\main.cpp backend\app.cpp backend\routes.cpp backend\config.cpp ^
  backend\core\*.cpp backend\db\*.cpp backend\services\*.cpp backend\handlers\*.cpp ^
  -I"%MYSQL%\include" -L"%MYSQL%\lib" -lmysql -lws2_32 -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic

if errorlevel 1 (
  echo.
  echo BUILD FAILED
  exit /b 1
)
echo Build OK: server.exe
