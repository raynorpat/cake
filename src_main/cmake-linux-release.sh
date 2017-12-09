cd ..
rm -rf build
mkdir build
cd build
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DONATIVE=ON -DSDL2=ON ../src_main
