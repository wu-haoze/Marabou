#!/usr/bin/zsh -e

USAGE="$0 <MAR-PATH> <MERGE-PATH> <SUBPROBLEM-DIR> <NET-PATH>"

rm -rf .gg

gg-init

MAR_PATH=${1?$USAGE}
MERGE_PATH=${2?$USAGE}
SUBPROBLEM_DIR=${3?$USAGE}
NET_PATH=${4?$USAGE}

gg-collect $MAR_PATH $NET_PATH $MAR_PATH $MERGE_PATH >/dev/null

MAR_HASH=$(gg-hash $MAR_PATH)
NET_HASH=$(gg-hash $NET_PATH)
MERGE_HASH=$(gg-hash $MERGE_PATH)

subhashes=$(for subproblem in $(ls $SUBPROBLEM_DIR); do
    gg-collect "${SUBPROBLEM_DIR}/${subproblem}" >/dev/null
    gg-create-thunk \
        --executable ${MAR_HASH} \
        --output out \
        --value $NET_HASH \
        --value $(gg-hash "${SUBPROBLEM_DIR}/${subproblem}") \
        -- \
        ${MAR_HASH} Marabou "@{GGHASH:${NET_HASH}}" "@{GGHASH:$(gg-hash "${SUBPROBLEM_DIR}/${subproblem}")}" --summary-file=out
done 2>&1)

gg-create-thunk \
    --executable $MERGE_HASH \
    --output out \
    $(for f in $(echo $subhashes); do; echo --thunk $f; done) \
    --placeholder merge_output \
    -- \
    $MERGE_HASH merge "@{GGHASH:$MERGE_HASH}" $(for f in $(echo $subhashes); do; echo "@{GGHASH:$f}"; done)

