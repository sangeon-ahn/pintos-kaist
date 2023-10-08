cd userprog
make clean
make
cd build
source ../../activate
pintos -- -q run alarm-multiple
# cd threads
# make clean
# make
# cd build
# source ../../activate
# pintos -- -q run alarm-multiple