#!/usr/bin/env zsh
set -e

it=80
od=2
for trial in 0 1 2; do
    for prop in 10275 32063 3753 41028 54955; do
        for id in 4 3 2; do
            for infra in thread gg-local; do
                j=$((2 ** $id));
                echo $j
                ./runner.py run \
                    --specific \
                    --trial $trial \
                    --mnist \
                    --initial-divides $id \
                    --jobs $j \
                    --infra $infra \
                    --divide-strategy split-relu \
                    --initial-timeout $it \
                    --timeout-factor 1.5 \
                    --online-divides $od \
                    20x20 $prop;
            done
        done
    done
done
