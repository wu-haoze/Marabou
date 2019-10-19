#!/usr/bin/zsh -e

USAGE="$0 <N> <NET-PATH> <PROP-PATH>"

N=${1?$USAGE}
NET_PATH=${2?$USAGE}
PROP_PATH=${3?$USAGE}

SPLIT_DIR=splits
SUBPROBLEM_DIR=subproblem

rm -rf $SPLIT_DIR
mkdir $SPLIT_DIR
cd $SPLIT_DIR && ../../build/Marabou --dnc --initial-divides $N --divide-only "../${NET_PATH}" "../${PROP_PATH}";
cd ..

rm -rf $SUBPROBLEM_DIR
mkdir $SUBPROBLEM_DIR
for sub_problem in $(ls $SPLIT_DIR); do
    cat ${PROP_PATH} "${SPLIT_DIR}/${sub_problem}" > "${SUBPROBLEM_DIR}/${sub_problem}"
done;
