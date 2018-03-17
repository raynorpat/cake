del /s /q build
mkdir build
cd build
cmake -G "Visual Studio 14 Win64" -DWIN_UWP=ON .
pause