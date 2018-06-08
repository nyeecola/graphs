@echo off
cls
rmdir build /s /q
mkdir build
copy src\*.glsl build\
copy src\*.ttf build\
cl src\main.c /I.\include\ /MD /DWIN32 /EHsc /nologo /link lib\GLFW\glfw3.lib opengl32.lib user32.lib gdi32.lib msvcrt.lib shell32.lib /OUT:"build/main_console.exe" /SUBSYSTEM:CONSOLE
cl src\main.c /I.\include\ /MD /DWIN32 /EHsc /nologo /link lib\GLFW\glfw3.lib opengl32.lib user32.lib gdi32.lib msvcrt.lib shell32.lib /OUT:"build/main.exe" /SUBSYSTEM:WINDOWS /ENTRY:"mainCRTStartup"
pushd build
main_console.exe
popd
