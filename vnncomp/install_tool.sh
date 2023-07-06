#! /usr/bin/env bash                                                                                                                                                                                               

apt-get -y install protobuf-compiler libprotoc-dev cmake

script_name=$(realpath $0)
script_path=$(dirname "$script_name")
project_path=$(dirname "$script_path")

echo Project path $project_path

pip3 install -r "$script_path"/requirements.txt
pip3 install "$script_path"/onnx2pytorch 

home=$project_path"/opt"
mkdir $home
export INSTALL_DIR="$home"
export GUROBI_HOME="$home/gurobi951/linux64"
export PATH="${PATH}:${GUROBI_HOME}/bin"
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${GUROBI_HOME}/lib"
export GRB_LICENSE_FILE="$home/gurobi.lic"

if [[ ! -d $home/gurobi951/ ]]
then
    wget https://packages.gurobi.com/9.5/gurobi9.5.1_linux64.tar.gz
    tar xvfz gurobi9.5.1_linux64.tar.gz -C $INSTALL_DIR
fi

cd $INSTALL_DIR/gurobi951/linux64/src/build
make
cp libgurobi_c++.a ../../lib/

echo "Project dir:" $project_path
cd $project_path

# install marabou

rm -rf build
mkdir build
cd build
cmake ../ -DENABLE_GUROBI=ON -DCMAKE_BUILD_TYPE=DEBUG
make -j48
cd ../

echo "lic:" $GRB_LICENSE_FILE
grbprobe
