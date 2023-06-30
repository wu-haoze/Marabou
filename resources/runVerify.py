import onnx
import onnxruntime as ort
import numpy as np
import random
import pickle
import sys
import os
import sys
import pathlib

sys.path.insert(0, os.path.join(str(pathlib.Path(__file__).parent.absolute()), "../"))

from maraboupy import MarabouCore
from maraboupy import Marabou

assert(len(sys.argv) == 4)

# python runSample.py [ipq_file.pickle] [vnnlib] [output file]

# output_props : List[Dict[index:float], float]
with open(sys.argv[1], "rb") as f:
    max_query_id, queriesMap, inputVarsMap, outputVarsMap  = pickle.load(f)

# output_props : List[Dict[index:float], float]
with open(sys.argv[2], "rb") as f:
    specs = pickle.load(f)

output_file = sys.argv[3]

options = Marabou.createOptions(solveWithMILP=True)
if max_query_id == 1:
    inputVars = inputVarsMap[1]
    queryName = queriesMap[1]
    ipq = Marabou.loadQuery(queryName)
    result, vals, stats = MarabouCore.solve(ipq, options=options)
    print(result)
else:
    for i in range(max_query_id + 1)[1:]:
        print(f"Calculating bound for query {i}")
        queryName = queriesMap[i]
        inputVars = inputVarsMap[i]
        outputVars = outputVarsMap[i]
        ipq = Marabou.loadQuery(queryName)
        if i < max_query_id:
            if i > 1:
                print(lastOutputBounds)
                print(f"Encoding bound from query {i-1}")
                assert(lastOutputShape==inputVars[0].shape)
                inputBounds = dict()
                for i, v in enumerate(inputVars[0].flatten()):
                    ipq.setLowerBound(v, lastOutputBounds[i][0])
                    ipq.setUpperBound(v, lastOutputBounds[i][1])
            result, bounds, stats = MarabouCore.calculateBounds(ipq, options=options)
            lastOutputBounds = np.array([bounds[v] for v in outputVars[0].flatten()])
            lastOutputShape = outputVars[0].shape
        else:
            print(lastOutputBounds)
            assert(lastOutputShape==inputVars[0].shape)
            inputBounds = dict()
            for i, v in enumerate(inputVars[0].flatten()):
                ipq.setLowerBound(v, lastOutputBounds[i][0])
                ipq.setUpperBound(v, lastOutputBounds[i][1])
            result, vals, stats = MarabouCore.solve(ipq, options=options)
        print(result)

if result == "unsat":
    with open(output_file, 'w') as out_file:
        out_file.write("unsat\n")
    exit(20)
elif result == "sat":
    with open(output_file, 'w') as out_file:
        out_file.write("sat\n")

    exit(10)
else:
    exit(1)
