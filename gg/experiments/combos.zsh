#!/usr/bin/env zsh
set -e

for ito in 5 20; do
    for od in 2 3 4; do
        ./runner.py run --trial 1 --specific --initial-divides 9 --jobs 512 --infra gg-lambda --divide-strategy split-relu --initial-timeout $ito --timeout-factor 1.5  --online-divides $od ../mnist/mnist20x20.nnet ../mnist-properties/net20x20_ind59248_tar8_eps0.008.txt;
    done
done
