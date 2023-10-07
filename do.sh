cd userprog
make clean
make
cd build
source ../../activate

pintos --fs-disk=10 -p tests/userprog/args-single:args-single -- -q -f run 'args-single onearg'


#pintos --fs-disk=10 -p tests/userprog/args-single:args-multiple -- -q -f run 'args-multiple onearg asdad'

#pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/exit:exit -- -q   -f run exit