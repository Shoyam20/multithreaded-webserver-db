@echo off
REM Starts the website at http://localhost:8080  (Ctrl+C to stop)
REM The MySQL password is NOT stored in the code. Set it first, in the same window:
REM     set ECOM_DB_PASS=your_mysql_password
if "%ECOM_DB_PASS%"=="" echo WARNING: ECOM_DB_PASS is not set - MySQL login will fail if your root user has a password.
server.exe
