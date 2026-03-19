@echo off

echo 正在编译设置
windres res\lg-reminder.rc o\rc.o

echo.
echo 正在编译主程序
g++ -c res\lg-reminder.cpp -o o\lg-reminder.o

echo.
echo 正在链接主程序
g++ o\lg-reminder.o o\rc.o -o lg-reminder\lg-reminder.exe -luser32 -lshell32 -lwinhttp -static

echo.
<nul set /p="打包完成，"
pause