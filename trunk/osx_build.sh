#! /bin/sh

# A noddy script to automate production of universal binaries on OS X.

make distclean &>/dev/null

export LDFLAGS="-Wl,-syslibroot,/Developer/SDKs/MacOSX10.3.9.sdk"
export CC="gcc -arch ppc -isysroot /Developer/SDKs/MacOSX10.3.9.sdk"
export CXX="g++ -arch ppc -isysroot /Developer/SDKs/MacOSX10.3.9.sdk"
./configure --with-internal-libs
make || exit
mv onscripter onscripter.ppc
make distclean &>/dev/null

export LDFLAGS="-Wl,-syslibroot,/Developer/SDKs/MacOSX10.4u.sdk"
export CC="gcc -arch i386 -isysroot /Developer/SDKs/MacOSX10.4u.sdk"
export CXX="g++ -arch i386 -isysroot /Developer/SDKs/MacOSX10.4u.sdk"
./configure --with-internal-libs
make || exit
mv onscripter onscripter.intel

lipo -create \
     -arch ppc onscripter.ppc \
     -arch i386 onscripter.intel \
     -output onscripter
