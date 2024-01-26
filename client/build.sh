sudo apt-get update -y;
sudo apt-get install clang ninja-build cmake pkg-config libssl-dev -y;
sudo cmake -B linuxbuild -G "Ninja" -DCMAKE_C_COMPILER=clang;
