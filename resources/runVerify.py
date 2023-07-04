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

assert(len(sys.argv) == 5)

# python runSample.py [ipq_file.pickle] [onnx_file] [vnnlib] [output file]

# output_props : List[Dict[index:float], float]
with open(sys.argv[1], "rb") as f:
    max_query_id, queriesMap, inputVarsMap, outputVarsMap  = pickle.load(f)

onnx_network = sys.argv[2]

# output_props : List[Dict[index:float], float]
with open(sys.argv[3], "rb") as f:
    specs = pickle.load(f)

output_file = sys.argv[4]

# Define the output property as a function that returns True or False
def output_property_hold(outputs, output_specs):
    outputs = outputs.flatten()
    # go through each disjunct
    for output_props, rhss in output_specs:
        # go through each conjunct
        hold = True
        for index in range(len(rhss)):
            output_prop, rhs = output_props[index], rhss[index]
            if sum([outputs[i] * c for i, c in output_prop.items()]) > rhs:
                hold = False
                break
        if hold:
            return True
    return False

options = Marabou.createOptions(solveWithMILP=True)
if max_query_id == 1:
    inputVars = inputVarsMap[1]
    queryName = queriesMap[1]
    ipq = Marabou.loadQuery(queryName)
    result, vals, stats = MarabouCore.solve(ipq, options=options)
    if result == "sat":
        # Load the onnx model
        sess_opt = ort.SessionOptions()
        sess_opt.intra_op_num_threads = 2
        sess_opt.inter_op_num_threads = 2
        ort_model = ort.InferenceSession(onnx_network, sess_opt)
        name, shape, dtype = [(i.name, i.shape, i.type) for i in ort_model.get_inputs()][0]
        if shape[0] in ["batch_size", "unk__195"]:
            shape[0] = 1
        assert dtype in ['tensor(float)', 'tensor(double)']
        dtype = "float32" if dtype == 'tensor(float)' else "float64"

        assignments = [vals[i] for i in inputVars[0].flatten()]
        ort_outputs = ort_model.run(None, {name: np.array([assignments]).astype(dtype).reshape(shape)})[0]
        input_spec, output_specs = specs[0]
        if output_property_hold(ort_outputs, output_specs):
            print("ONNX test passed!")
        else:
            print("ONNX test failed!")
            result = "unknown"
else:
    for i in range(max_query_id + 1)[1:]:
        print(f"Calculating bound for query {i}")
        queryName = queriesMap[i]
        inputVars = inputVarsMap[i]
        outputVars = outputVarsMap[i]
        print("Loading query...")
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
            assert(lastOutputShape==inputVars[0].shape)
            inputBounds = dict()
            for i, v in enumerate(inputVars[0].flatten()):
                ipq.setLowerBound(v, lastOutputBounds[i][0])
                ipq.setUpperBound(v, lastOutputBounds[i][1])
            result, vals, stats = MarabouCore.solve(ipq, options=options)
        del ipq
    if result == "unsat":
        result == "unsat"
    else:
        result = "unknown"

if result == "unsat":
    with open(output_file, 'w') as out_file:
        out_file.write("unsat\n")
    exit(20)
elif result == "sat":
    res = "sat"
    for index, x in enumerate(assignments):
        if index == 0:
            res += "\n("
        else:
            res += "\n "

        res += f"(X_{index} {x})"

    with open(output_file, 'w') as out_file:
        out_file.write(res)
    exit(10)
else:
    exit(30)
