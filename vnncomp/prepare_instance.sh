#!/bin/bash

BENCHMARK=$2
ONNX_FILE=$3
VNNLIB_FILE=$4
NET_NAME=$(basename $ONNX_FILE)
PROP_NAME=$(basename $VNNLIB_FILE)

echo "Solving benchmark set '$BENCHMARK' with onnx file '$ONNX_FILE' and vnnlib file '$VNNLIB_FILE'"
echo "Network name: $NET_NAME"
echo "Property name: $PROP_NAME"

####### Creating working directories #######

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
WORKING_DIR_BENCHMARK=$SCRIPT_DIR/data/$BENCHMARK/
WORKING_DIR_INSTANCE=$WORKING_DIR_BENCHMARK/"$NET_NAME"_"$PROP_NAME"/

if [[ ! -d $WORKING_DIR_BENCHMARK ]]
then
    mkdir -p $WORKING_DIR_BENCHMARK
fi
if [[ -d $WORKING_DIR_INSTANCE ]]
then
    rm -rf $WORKING_DIR_INSTANCE
fi
mkdir $WORKING_DIR_INSTANCE
echo "Working directory: $WORKING_DIR_INSTANCE"

####### Simplify #######
ONNX_FILE_SIMP=$WORKING_DIR_BENCHMARK/$NET_NAME.simp
if [[ ! -f $ONNX_FILE_SIMP ]]
then
    python3 -m onnxsim $ONNX_FILE $ONNX_FILE_SIMP
fi
if [[ -f $ONNX_FILE_SIMP ]]
then
   echo "Simplified ONNX file: $ONNX_FILE_SIMP"
else
    echo "Simplified ONNX file NOT FOUND!"
    exit 1
fi

###### Remove Softmax ######
ONNX_FILE_PRESOFTMAX="$ONNX_FILE_SIMP".presoftmax
if [[ ! -f $ONNX_FILE_PRESOFTMAX ]]
then
    python3 process_network/get_presoftmax_network.py $ONNX_FILE_SIMP $ONNX_FILE_PRESOFTMAX
fi
if [[ -f $ONNX_FILE_PRESOFTMAX ]]
then
    echo "Simplified ONNX file: $ONNX_FILE_SIMP"
else
    echo "Simplified ONNX file NOT FOUND!"
    exit 1
fi

exit 0

# Convert large networks
echo "TODO: convert large networks..."
${TOOL_DIR}/vnncomp_scripts/maxpool_to_relu.py $ONNX_FILE

# Warmup, using a 1 second timeout.
echo
echo "TODO: running warmup..."
echo

home="/opt"
export INSTALL_DIR="$home"
export GUROBI_HOME="$home/gurobi951/linux64"
export PATH="${PATH}:${GUROBI_HOME}/bin"
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${GUROBI_HOME}/lib"
export GRB_LICENSE_FILE="$home/gurobi.lic"

# kill any remaining python processes.
pkill -9 Marabou
pkill -9 python
pkill -9 python3
sleep 2
pkill -9 Marabou
pkill -9 python
pkill -9 python3
sleep 2
echo "Preparation finished."

# script returns a 0 exit code if successful. If you want to skip a benchmark category you can return non-zero.
exit 0
