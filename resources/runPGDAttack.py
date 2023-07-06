import onnx
import onnxruntime as ort
import numpy as np
import random
import pickle
import sys
from onnx2pytorch import ConvertModel
import torch
import os
os.environ["OMP_NUM_THREADS"] = "1"
torch.set_num_interop_threads(4)
torch.set_num_threads(4)

assert(len(sys.argv) == 7)

# python runPGDAttack.py [network] [spec pickle] [output file] [seed] [alpha] [attempts]

onnx_network = sys.argv[1]

output_file = sys.argv[3]

onnx_model = onnx.load(onnx_network)
pytorch_model = ConvertModel(onnx_model)
pytorch_model.eval()
pytorch_model.to("cpu")

# output_props : List[Dict[index:float], float]
with open(sys.argv[2], "rb") as f:
    specs = pickle.load(f)

seed = int(sys.argv[4])
np.random.seed(seed)
random.seed(seed)

alpha = float(sys.argv[5])

attempts = int(sys.argv[6])

print(f"PGD Attack using random seed {seed} with {attempts} attempts, writing results to {output_file}")

# Load the onnx model
sess_opt = ort.SessionOptions()
sess_opt.intra_op_num_threads = 4
sess_opt.inter_op_num_threads = 4
ort_model = ort.InferenceSession(onnx_network, sess_opt)
name, shape, dtype = [(i.name, i.shape, i.type) for i in ort_model.get_inputs()][0]
if isinstance(shape[0], str):
    shape[0] = 1
assert dtype in ['tensor(float)', 'tensor(double)']
dtype = "float32" if dtype == 'tensor(float)' else "float64"

def convert_output_spec_to_loss(outputs, output_spec):
    flattened_outputs = outputs.flatten()
    output_props, rhss = output_spec
    loss = 0
    for output_prop in output_props:
        for i, c in output_prop.items():
            loss += c * flattened_outputs[i]
    return loss

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

def pgd_attack(model, input_spec, output_spec, steps=20, device="cpu"):
    #print("output spec", output_spec)
    random_point = [np.random.uniform(low, high) for low, high in input_spec]
    images = np.array(random_point, dtype=dtype).reshape(shape)
    images = torch.from_numpy(images)
    adv_images = images.clone().detach().to(device)

    min_list = torch.tensor([lb for lb, _ in input_spec]).reshape(shape)
    max_list = torch.tensor([ub for lb, ub in input_spec]).reshape(shape)
    perturbation = torch.tensor(np.array([alpha * (ub - lb) for lb, ub in input_spec]).reshape(shape))
    costs = []
    for step in range(steps):
        adv_images.requires_grad = True
        outputs = model(adv_images)
        ort_outputs = ort_model.run(None, {name: adv_images.detach().numpy().astype(dtype)})[0]
        #print("diff:", ort_outputs - outputs.detach().numpy())
        #print(ort_outputs)
        if output_property_hold(ort_outputs, output_specs):
            return adv_images.detach().numpy().flatten(), ort_outputs

        cost = convert_output_spec_to_loss(outputs, output_spec)
        costs.append(cost.detach().numpy())
        # Update adversarial images
        grad = torch.autograd.grad(cost, adv_images,
                                   retain_graph=False, create_graph=False)[0]
        #print(grad)

        adv_images = adv_images.detach() - perturbation*grad.sign()
        adv_images = torch.clip(adv_images, min_list, max_list).to(torch.float32)
    #print(costs)
    return None

for _ in range(attempts):
    # Generate a random point within the input range
    index = np.random.randint(len(specs))
    box_spec = specs[index]
    input_spec, output_specs = box_spec
    output_spec = random.choice(output_specs)
    # pgd attack
    result = pgd_attack(pytorch_model, input_spec, output_spec)
    if result is not None:
        point, output = result
        print(f"Satisfying assignment found. Input spec {index}, {point} {output}")
        res = 'sat'
        for index, x in enumerate(point):
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
        break
