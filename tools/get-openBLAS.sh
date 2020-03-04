#!/bin/bash
curdir=$pwd
mydir="${0%/*}"

cd tools/
wget http://github.com/xianyi/OpenBLAS/archive/v0.3.6.tar.gz
tar -xzf v0.3.6.tar.gz >> /dev/null
mv OpenBLAS-0.3.6/ OpenBLAS/
cd OpenBLAS/
make NO_SHARED=1 CBLAS_ONLY=1 USE_THREAD=0 -j4
mkdir installed/
make PREFIX=installed/ NO_SHARED=1 install
rm ../v0.3.6.tar.gz
cd $curdir
