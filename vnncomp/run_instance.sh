#!/bin/bash

VERSION_STRING=v1

# check arguments
if [ "$1" != ${VERSION_STRING} ]; then
	echo "Expected first argument (version string) '$VERSION_STRING', got '$1'"
	exit 1
fi

BENCHMARK=$2
ONNX_FILE=$3
VNNLIB_FILE=$4
RESULTS_FILE=$5
TIMEOUT=10

echo "Running benchmark instance in category '$BENCHMARK' with onnx file '$ONNX_FILE', vnnlib file '$VNNLIB_FILE', results file $RESULTS_FILE, and timeout $TIMEOUT"

NET_NAME=$(basename ${ONNX_FILE%.onnx})
PROP_NAME=$(basename ${VNNLIB_FILE%.vnnlib})

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
WORKING_DIR_BENCHMARK=$SCRIPT_DIR/data/$BENCHMARK/
WORKING_DIR_INSTANCE=$WORKING_DIR_BENCHMARK/"$NET_NAME"_"$PROP_NAME"/

ONNX_FILE_POSTDNNV=$WORKING_DIR_BENCHMARK/"$NET_NAME"_simp_presoftmax_postdnnv.onnx

VNNLIB_FILE_PICKLED=$WORKING_DIR_INSTANCE/vnnlib.pkl
IPQ_FILE=$WORKING_DIR_INSTANCE/query.ipq

for i in {1..100}
do
    if [[ -f "$RESULTS_FILE"_"$i" ]]
    then
        rm "$RESULTS_FILE"_"$i"
    fi
done


# Run the processes in the background and store their PIDs in an array
pids=()
#python3 -u $SCRIPT_DIR/../resources/runSample.py $ONNX_FILE_POSTDNNV $VNNLIB_FILE_PICKLED "$RESULTS_FILE"_1 1 10000000000 &
#pids+=($!)
#python3 -u $SCRIPT_DIR/../resources/runSample.py $ONNX_FILE_POSTDNNV $VNNLIB_FILE_PICKLED "$RESULTS_FILE"_2 2 10000000000 &
#pids+=($!)
# big step
#python3 -u $SCRIPT_DIR/../resources/runPGDAttack.py $ONNX_FILE_POSTDNNV $VNNLIB_FILE_PICKLED "$RESULTS_FILE"_3 3 0.1 10000000 &
#pids+=($!)
# small step
#python3 -u $SCRIPT_DIR/../resources/runPGDAttack.py $ONNX_FILE_POSTDNNV $VNNLIB_FILE_PICKLED "$RESULTS_FILE"_4 4 0.05 10000000 &
#pids+=($!)
$SCRIPT_DIR/../build/Marabou --input-query $WORKING_DIR_INSTANCE/query.ipq --milp --num-workers 64 &
pids+=($!)
#python3 -u $SCRIPT_DIR/../resources/runPGDAttack.py $ONNX_FILE_POSTDNNV $VNNLIB_FILE_PICKLED "$RESULTS_FILE"_4 4 0.05 10000000 &
#pids+=($!)


for pid in "${pids[@]}"; do
    echo "Process id $pid"
done


#TIMEOUT=$((TIMEOUT + 1))

# loop until one of the subprocesses exits with code 10 or 20, or all of them exit
while true; do
    # check the exit codes of the subprocesses
    exit_codes=()
    for pid in "${pids[@]}"; do
        exit_codes+=($(wait $pid 2>/dev/null; echo $?))
    done
    # if any of them is 10 or 20, break the loop
    if [[ " ${exit_codes[@]} " =~ " 10 " ]] || [[ " ${exit_codes[@]} " =~ " 20 " ]]; then
        echo "Problem solved!"
        break
    fi
    elapsed=$(ps -o etimes= -p $$) # Get the elapsed time of this script
    if [ $elapsed -ge $TIMEOUT ]; then
        echo "Timeout reached"
        break # Exit the loop
    fi
    sleep 1 # Wait for a second before checking again
done

# kill the remaining subprocesses
for pid in "${pids[@]}"; do
    kill $pid 2>/dev/null
done

pkill -9 python3
pkill -9 python

# do some cleanup work
echo "Cleanup done"

exit_code=0

for i in {1..100}
do
    filename="$RESULTS_FILE"_"$i"
    # Check if the file exists
    if [ -f $filename ]; then
        echo Found $(realpath "$filename")
        # Get the first line of the file
        first_line=$(head -n 1 "$filename")
        # Check if the first line is "sat" or "unsat"
        if [ "$first_line" == "sat" ]; then
            cp "$filename" "$RESULTS_FILE"
            echo "sat"
        elif [ "$first_line" == "unsat" ]; then
            cp "$filename" "$RESULTS_FILE"
            echo "unsat"
        fi
    fi
done
