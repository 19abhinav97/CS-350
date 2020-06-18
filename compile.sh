#bin/bash
# A2

if [ -z "$1" ]; then
    echo "Usage:"
    echo "  ./compile <assignment number> [-user] [-run]"
    echo ""
    echo "Examples: './compile 0' will compile ASST0"
    echo "Examples: './compile 0 -user' will compile ASST0 and the userspace"
    echo "          './compile 0 -run' will compile ASST0 and run ASST0"
    echo "          './compile 0 -user -run' will compile ASST0 and the userspace, and run ASST0"
    echo ""
else
   echo 'Building OS161'
   cd ~/cs350-os161/os161-1.99/kern/compile
   cd ASST$1
   bmake depend && bmake && bmake install
   cd ~/cs350-os161/os161-1.99

   if echo "$@" | grep "user"; then
    bmake
    bmake install
   fi

   if echo "$@" | grep "run"; then
       cd ~/cs350-os161/root
       sys161 kernel-ASST$1
   fi
fi
