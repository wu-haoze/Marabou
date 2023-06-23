import onnx
import onnxruntime as ort
import numpy as np
import random
import pickle
import sys


assert(len(sys.argv) == 6)

# python runSample.py [network] [spec pickle] [output file] [seed] [samples]

onnx_network = sys.argv[1]

# output_props : List[Dict[index:float], float]
with open(sys.argv[2], "rb") as f:
    specs = pickle.load(f)

output_file = sys.argv[3]

seed = int(sys.argv[4])
print(f"Using random seed {seed}")
np.random.seed(seed)
random.seed(seed)

num_samples = int(sys.argv[5])
print(f"Sampling {num_samples} points")

# Define the output property as a function that returns True or False
def output_property_hold(outputs, output_specs):
    outputs = outputs.flatten()
    print(output_specs)
    for output_props, rhss in output_specs:
        hold = True
        for i in range(len(rhss)):
            output_prop, rhs = output_props[i], rhss[i]
        if sum([outputs[i] * c for i, c in output_prop.items()]) <= rhs:
            return True
    return False

# Load the onnx model
model = ort.InferenceSession(onnx_network)
name, shape, dtype = [(i.name, i.shape, i.type) for i in model.get_inputs()][0]
assert dtype in ['tensor(float)', 'tensor(double)']
dtype = "float32" if dtype == 'tensor(float)' else "float64"

for _ in range(num_samples):
    # Generate a random point within the input range
    index = np.random.randint(len(specs))
    box_spec = specs[index]
    input_spec, output_specs = box_spec
    point = np.array([np.random.uniform(low, high) for low, high in input_spec],  dtype=dtype).reshape(shape) # check if reshape order is correct
    # Run the model on the point
    output = model.run(None, {name: point})[0]
    # Check if the output satisfies the property
    if output_property_hold(output, output_specs):
        res = 'sat'
        names = [i.name for i in sess.get_inputs()]
        for index, x in enumerate(len(input_specs[0])):
            if index == 0:
                res += "\n("
            else:
                res += "\n "

            res += f"(X_{index} {x})"

        # next print the Y values
        for index, x in enumerate(flat_out):
            res += f"\n (Y_{index} {x})"

        res += ")\n"
        break
