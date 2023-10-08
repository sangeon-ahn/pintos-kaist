cd userprog
cd build
source ../../activate

# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/create-bound:create-bound -- -q   -f run create-bound < /dev/null 2> tests/userprog/create-bound.errors > tests/userprog/create-bound.output
# perl -I../.. ../../tests/userprog/create-bound.ck tests/userprog/create-bound tests/userprog/create-bound.result

pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/create-empty:create-empty -- -q   -f run create-empty < /dev/null 2> tests/userprog/create-empty.errors > tests/userprog/create-empty.output
perl -I../.. ../../tests/userprog/create-empty.ck tests/userprog/create-empty tests/userprog/create-empty.result