mkdir -p bin;
mkdir -p build;

mkdir -p logs/access;
mkdir -p logs/info;

mkdir -p public;

cmake -B build -DCMAKE_BUILD_TYPE=Debug;
cmake --build build --config Debug;
