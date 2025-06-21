# $1 = arch
# $2 = sysroot

if [[ -d build-freetype ]]; then
    echo Assuming Freetype is up to date
    exit 0
fi 

mkdir build-freetype
cd build-freetype
../freetype-2.4.9/configure --host=$1-ethereal --prefix=/usr
make -j4
make DESTDIR=$2 install
cd ..