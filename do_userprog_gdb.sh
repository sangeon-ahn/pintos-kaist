cd userprog
make clean
make
cd build
source ../../activate
pintos-mkdisk filesys.dsk 10
pintos --gdb --fs-disk=10 -p tests/userprog/exit:exit -- -q -f run 'exit'
# pintos --gdb --fs-disk=10 -p tests/userprog/fork-once:fork-once -- -q -f run 'fork-once'
# pintos --gdb --fs-disk=10 -p tests/userprog/fork-multiple:fork-multiple -- -q -f run 'fork-multiple'