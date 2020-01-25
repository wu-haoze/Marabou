#!/usr/bin/env zsh

it=80
od=2
for trial in 0 1 2; do
    for id in 10; do
        for prop in 10275 32063 3753 41028 54955; do
            for infra in gg-lambda; do
                j=1000
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
