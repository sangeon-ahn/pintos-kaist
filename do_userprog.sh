cd userprog
make clean
make
cd build
source ../../activate
pintos-mkdisk filesys.dsk 10 
# pintos --fs-disk filesys.dsk -p tests/userprog/args-single:args-single -- -q -f run 'args-single onearg'
# pintos --fs-disk=10 -p tests/userprog/args-multiple:args-multiple -- -q -f run 'args-multiple ab cd ef gh'
# pintos --fs-disk=10 -p tests/userprog/halt:halt -- -q -f run 'halt'
# pintos --fs-disk=10 -p tests/userprog/exit:exit -- -q -f run 'exit'
# pintos --fs-disk=10 -p tests/userprog/fork-once:fork-once -- -q -f run 'fork-once'
pintos --fs-disk=10 -p tests/userprog/fork-multiple:fork-multiple -- -q -f run 'fork-multiple'
# pintos --fs-disk=10 -p tests/userprog/fork-recursive:fork-recursive -- -q -f run 'fork-recursive'
# pintos --fs-disk=10 -p tests/userprog/wait-simple:wait-simple -- -q -f run 'wait-simple'