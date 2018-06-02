@echo off
cls
rmdir build /s /q
mkdir build
copy lib\GLFW\glfw3.dll build\
cl src\main.c /I.\include\ /EHsc /nologo /link lib\GLFW\glfw3dll.lib /OUT:"build/main.exe"
build\main.exe
