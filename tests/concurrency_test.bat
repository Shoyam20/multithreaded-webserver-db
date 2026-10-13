@echo off
setlocal
REM ---------------------------------------------------------------------------------
REM Concurrency test: two shoppers press "Place order" at the same moment for the LAST unit
REM of a product. Expected result: exactly ONE order is created and the stock is 0 (never negative).
REM
REM Needs: MySQL running, server.exe running (http://localhost:8080), curl, and in THIS window:
REM     set ECOM_DB_PASS=your_mysql_password
REM Usage:   tests\concurrency_test.bat [product_id]        (default product 2)
REM The script creates two temporary users, and deletes them and their orders afterwards.
REM ---------------------------------------------------------------------------------

set PRODUCT=%1
if "%PRODUCT%"=="" set PRODUCT=2

set SERVER=localhost:8080
set "PATH=%PATH%;C:\Program Files\MySQL\MySQL Server 8.0\bin"
set MYSQL=mysql
set MYSQL_PWD=%ECOM_DB_PASS%
set RUN=%RANDOM%
set U1=race1_%RUN%@example.com
set U2=race2_%RUN%@example.com
set DIR=%TEMP%\serversync_race_%RUN%
mkdir "%DIR%"

echo Remembering the real stock of product %PRODUCT% ...
for /f %%s in ('%MYSQL% -uroot -N -e "SELECT stock FROM products WHERE product_id=%PRODUCT%" ecommerce_db') do set OLD_STOCK=%%s
if "%OLD_STOCK%"=="" (
  echo Could not read the product. Is MySQL running and ECOM_DB_PASS set?
  exit /b 1
)
echo   stock before the test: %OLD_STOCK%

echo Creating two users and logging them in ...
curl -s -o nul -d "name=Race+One&email=%U1%&password=pw12345" http://%SERVER%/signup
curl -s -o nul -d "name=Race+Two&email=%U2%&password=pw12345" http://%SERVER%/signup
curl -s -o nul -c "%DIR%\c1.txt" -d "email=%U1%&password=pw12345" http://%SERVER%/login
curl -s -o nul -c "%DIR%\c2.txt" -d "email=%U2%&password=pw12345" http://%SERVER%/login

echo Setting the stock to 1 and putting the product in both carts ...
%MYSQL% -uroot -e "UPDATE products SET stock=1 WHERE product_id=%PRODUCT%" ecommerce_db
curl -s -o nul -b "%DIR%\c1.txt" -d "product_id=%PRODUCT%&quantity=1" http://%SERVER%/cart/add
curl -s -o nul -b "%DIR%\c2.txt" -d "product_id=%PRODUCT%&quantity=1" http://%SERVER%/cart/add

echo Placing both orders at the same time ...
REM One curl process, two transfers started together (--parallel --parallel-immediate).
curl -s --parallel --parallel-immediate -o "%DIR%\page1.html" -b "%DIR%\c1.txt" -X POST -w "user1: HTTP %%{http_code} -> %%{redirect_url}\n" http://%SERVER%/checkout/place --next -o "%DIR%\page2.html" -b "%DIR%\c2.txt" -X POST -w "user2: HTTP %%{http_code} -> %%{redirect_url}\n" http://%SERVER%/checkout/place
ping -n 3 127.0.0.1 >nul

echo.
echo   (a redirect to /orders = order placed; HTTP 200 with "Not enough stock" = refused)

for /f %%n in ('%MYSQL% -uroot -N -e "SELECT COUNT(*) FROM orders o JOIN users u ON u.user_id=o.user_id WHERE u.email IN ('%U1%','%U2%')" ecommerce_db') do set ORDERS=%%n
for /f %%s in ('%MYSQL% -uroot -N -e "SELECT stock FROM products WHERE product_id=%PRODUCT%" ecommerce_db') do set NEW_STOCK=%%s

echo.
echo Orders created by the two users : %ORDERS%   (expected 1)
echo Stock of product %PRODUCT% afterwards   : %NEW_STOCK%   (expected 0)

set RESULT=FAIL
if "%ORDERS%"=="1" if "%NEW_STOCK%"=="0" set RESULT=PASS
echo.
echo RESULT: %RESULT%

echo.
echo Cleaning up: deleting the test users and orders, restoring the stock to %OLD_STOCK% ...
%MYSQL% -uroot -e "DELETE FROM payments WHERE order_id IN (SELECT order_id FROM orders WHERE user_id IN (SELECT user_id FROM users WHERE email IN ('%U1%','%U2%')))" ecommerce_db
%MYSQL% -uroot -e "DELETE FROM order_items WHERE order_id IN (SELECT order_id FROM orders WHERE user_id IN (SELECT user_id FROM users WHERE email IN ('%U1%','%U2%')))" ecommerce_db
%MYSQL% -uroot -e "DELETE FROM orders WHERE user_id IN (SELECT user_id FROM users WHERE email IN ('%U1%','%U2%'))" ecommerce_db
%MYSQL% -uroot -e "DELETE FROM cart_items WHERE cart_id IN (SELECT cart_id FROM cart WHERE user_id IN (SELECT user_id FROM users WHERE email IN ('%U1%','%U2%')))" ecommerce_db
%MYSQL% -uroot -e "DELETE FROM cart WHERE user_id IN (SELECT user_id FROM users WHERE email IN ('%U1%','%U2%'))" ecommerce_db
%MYSQL% -uroot -e "DELETE FROM user_sessions WHERE user_id IN (SELECT user_id FROM users WHERE email IN ('%U1%','%U2%'))" ecommerce_db
%MYSQL% -uroot -e "DELETE FROM users WHERE email IN ('%U1%','%U2%')" ecommerce_db
%MYSQL% -uroot -e "UPDATE products SET stock=%OLD_STOCK% WHERE product_id=%PRODUCT%" ecommerce_db
rmdir /s /q "%DIR%"

if "%RESULT%"=="PASS" (exit /b 0) else (exit /b 1)
