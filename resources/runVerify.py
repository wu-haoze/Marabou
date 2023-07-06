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

assert(len(sys.argv) == 6)

# python runSample.py [ipq_file.pickle] [onnx_file] [vnnlib] [output file] [mode]

if "vgg16-7" in os.path.basename(sys.argv[2]):
    with open(sys.argv[4], 'w') as out_file:
        out_file.write("unknown")
    exit(0)

# output_props : List[Dict[index:float], float]
with open(sys.argv[1], "rb") as f:
    max_query_id, queriesMap, inputVarsMap, outputVarsMap  = pickle.load(f)

onnx_network = sys.argv[2]

# output_props : List[Dict[index:float], float]
with open(sys.argv[3], "rb") as f:
    specs = pickle.load(f)

output_file = sys.argv[4]

mode = sys.argv[5]

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


MODE_MILP = 1
MODE_SNC = 2
MODE_PORTFOLIO = 3
MODE_MILP2 = 4
MODE_MILP3 = 5

if max_query_id > 1 and len(outputVarsMap) == 0:
    print("Input disjunction detected!")
    for query_id in range(max_query_id + 1)[1:]:
        inputVars = inputVarsMap[query_id]
        queryName = queriesMap[query_id]
        ipq = Marabou.loadQuery(queryName)
        if ipq.getNumberOfVariables() < 5000 and ipq.getNumInputVariables() <= 10:
            if mode == "default":
                mode = MODE_SNC
            else:
                with open(sys.argv[4], 'w') as out_file:
                    out_file.write("unknown")
                exit(0)
        elif mode == "default":
            mode = MODE_MILP
        else:
            with open(sys.argv[4], 'w') as out_file:
                out_file.write("unknown")
            exit(0)

        result, vals, stats = MarabouCore.solve(ipq, mode=mode)
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
                break
        elif result == "unsat":
            continue
        else:
            result = "unknown"
            break

elif max_query_id == 1:
    inputVars = inputVarsMap[1]
    queryName = queriesMap[1]
    ipq = Marabou.loadQuery(queryName)

    if ipq.getNumberOfVariables() < 2000 and ipq.getNumInputVariables() < 10:
        if mode == "default":
            mode = MODE_SNC
        else:
            mode = MODE_PORTFOLIO
    elif mode == "default":
        mode = MODE_MILP
    else:
        exit(0)

    result, vals, stats = MarabouCore.solve(ipq, mode=mode)
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

    if result not in ["sat", "unsat"]:
        mode = MODE_MILP2
        result, vals, stats = MarabouCore.solve(ipq, mode=mode)
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

    if result not in ["sat", "unsat"]:
        mode = MODE_MILP3
        result, vals, stats = MarabouCore.solve(ipq, mode=mode)
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
    if mode != "default":
        mode = MODE_MILP
    else:
        exit(0)

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
            result, bounds, stats = MarabouCore.calculateBounds(ipq)
            lastOutputBounds = np.array([bounds[v] for v in outputVars[0].flatten()])
            lastOutputShape = outputVars[0].shape
        else:
            assert(lastOutputShape==inputVars[0].shape)
            inputBounds = dict()
            for i, v in enumerate(inputVars[0].flatten()):
                ipq.setLowerBound(v, lastOutputBounds[i][0])
                ipq.setUpperBound(v, lastOutputBounds[i][1])
            result, vals, stats = MarabouCore.solve(ipq, mode=MODE_MILP)
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
    with open(sys.argv[4], 'w') as out_file:
        out_file.write("unknown")

    exit(30)
