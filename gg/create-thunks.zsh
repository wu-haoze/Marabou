#!/usr/bin/zsh -e

USAGE="$0 <MAR-PATH> <MERGE-PATH> <NET-PATH> <PROP-PATH> <INIT-DIVIDES> <ONLINE-DIVIDES> <TIMEOUT> <TIMEOUT-FACTOR>"

rm -rf .gg

gg-init

MAR_PATH=${1?$USAGE}
MERGE_PATH=${2?$USAGE}
NET_PATH=${3?$USAGE}
PROP_PATH=${4?${USAGE}}
INIT_DIVIDES=${5?Initial Divides)}
ONLINE_DIVIDES=${6?Online Divides)}
TIMEOUT=${7?Timeout}
TIMEOUT_FACTOR=${8?Timeout Factor}

gg-collect $MAR_PATH $NET_PATH $MERGE_PATH $PROP_PATH

MAR_HASH=$(gg-hash $MAR_PATH)
MERGE_HASH=$(gg-hash $MERGE_PATH)
NET_HASH=$(gg-hash $NET_PATH)
PROP_HASH=$(gg-hash $PROP_PATH)
OUT_PATH=out
QUERY_ID="${NET_PATH:t},${PROP_PATH:t}"
if [[ $INIT_DIVIDES -ne 0 ]]; then
    DIVIDES=$INIT_DIVIDES
else
    DIVIDES=$ONLINE_DIVIDES
fi

function placeholder() {
    echo "@{GGHASH:$1}"
}

function countargs() {
    echo "$#"
}

outputs=$(echo $OUT_PATH
for i in $(seq 1 $(( 2 ** DIVIDES ))); do
    echo $QUERY_ID-$i.prop
    echo $QUERY_ID-$i.thunk
done;)



gg-create-thunk \
    --executable $MAR_HASH \
    --executable $MERGE_HASH \
    --value $NET_HASH \
    --value $PROP_HASH \
    $(for o in "${=outputs}"; do; echo --output $o; done) \
    --placeholder out \
    -- \
    $MAR_HASH Marabou \
    --gg-output \
    --timeout $TIMEOUT \
    --timeout-factor $TIMEOUT_FACTOR \
    --initial-divides $INIT_DIVIDES \
    --num-online-divides $ONLINE_DIVIDES \
    --summary-file out \
    --merge-file $(placeholder $MERGE_HASH) \
    --self-hash $MAR_HASH \
    --query-id $QUERY_ID \
    $(placeholder $NET_HASH) \
    $(placeholder $PROP_HASH)

