import onnx
import onnxruntime as ort
import numpy as np
import random
import pickle
import sys
import os
import math
os.environ["OMP_NUM_THREADS"] = "1"

assert(len(sys.argv) == 6)

# python runSample.py [network] [spec pickle] [output file] [seed] [samples]

onnx_network = sys.argv[1]

# output_props : List[Dict[index:float], float]
with open(sys.argv[2], "rb") as f:
    specs = pickle.load(f)

output_file = sys.argv[3]

seed = int(sys.argv[4])
np.random.seed(seed)
random.seed(seed)

num_samples = int(sys.argv[5])
print(f"Enumerating {num_samples} points, using random seed {seed}, writing results to {output_file}")

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

# Load the onnx model
sess_opt = ort.SessionOptions()
sess_opt.intra_op_num_threads = 4
model = ort.InferenceSession(onnx_network, sess_opt)
name, shape, dtype = [(i.name, i.shape, i.type) for i in model.get_inputs()][0]
if shape[0] in ["batch_size", "unk__195"]:
    shape[0] = 1
assert dtype in ['tensor(float)', 'tensor(double)']
dtype = "float32" if dtype == 'tensor(float)' else "float64"

import itertools

if "vgg16-7" in os.path.basename(onnx_network):
    print("Vgg")
    # Generate a random point within the input range
    index = np.random.randint(len(specs))
    box_spec = specs[index]
    input_spec, output_specs = box_spec
    random_point = []
    counter = 0
    for i, (low, high) in enumerate(input_spec):
        if low < high:
            counter += 1
            random_point.append(list(np.linspace(low, high,
                                                 int((high - low) / 0.00000005))))
        else:
            random_point.append([low])
    if counter > 2:
        exit(0)
    points = list(itertools.product(*random_point))
    print(len(points))
    for point in points:
        point = np.array(list(point), dtype=dtype).reshape(shape) # check if reshape order is correct
        # Run the model on the point
        output = model.run(None, {name: point})[0]
        # Check if the output satisfies the property
        if output_property_hold(output, output_specs):
            print(f"Satisfying assignment found. Input spec {index}, {point} {output}")
            res = 'sat'
            for index, x in enumerate(random_point):
                if index == 0:
                    res += "\n("
                else:
                    res += "\n "

                res += f"(X_{index} {x})"

            # next print the Y values
            for index, x in enumerate(output.flatten()):
                res += f"\n (Y_{index} {x})"

            res += ")\n"
            with open(output_file, 'w') as out_file:
                print(f"Recording satisfying assignment to {output_file}!")
                out_file.write(res)
                exit(10)

    res = 'unsat'
    with open(output_file, 'w') as out_file:
        out_file.write(res)
        exit(20)
