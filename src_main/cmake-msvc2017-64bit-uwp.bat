del /s /q build
mkdir build
cd build
cmake -G "Visual Studio 15 Win64" -DCMAKE_SYSTEM_NAME="WindowsStore" -DCMAKE_SYSTEM_VERSION="10.0" -DMONOLITH=ON -DWIN_UWP=ON ..
pause