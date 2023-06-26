import onnx
from functools import reduce
import numpy as np
import sys
import pickle

from vnnlib import read_vnnlib_simple
import os
import pathlib

sys.path.insert(0, os.path.join(str(pathlib.Path(__file__).parent.absolute()), "../../"))

from maraboupy.MarabouNetworkONNX import *
from maraboupy import MarabouCore
from maraboupy.MarabouUtils import *

def parse_vnnlib_file(onnx_file, vnnlib_file, pickle_output, ipq_output):
    # Load the onnx model
    model = onnx.load(onnx_file)

    # Get the input and output nodes
    input_nodes = model.graph.input
    output_nodes = model.graph.output

    assert(len(input_nodes) == 1)
    assert(len(output_nodes) == 1)

    num_inputs = 1
    for d in input_nodes[0].type.tensor_type.shape.dim:
        num_inputs *= (d.dim_value if d.dim_value != 0 else 1)

    num_outputs = 1
    for d in output_nodes[0].type.tensor_type.shape.dim:
        num_outputs *= (d.dim_value if d.dim_value != 0 else 1)

    print(f"Testing onnx model with {num_inputs} inputs and {num_outputs} outputs")

    box_spec_list = read_vnnlib_simple(vnnlib_file, num_inputs, num_outputs)

    for box_spec in box_spec_list:
        input_box = box_spec[0]
        spec_list = box_spec[1]

        input_list = []

        assert(len(input_box) == num_inputs)

    # Open a file in binary write mode
    with open(pickle_output, "wb") as f:
        # Pickle the list and write it to the file
        pickle.dump(box_spec_list, f)

    network = create_marabou_query(onnx_file, box_spec_list)
    print("Number of relus: {}".format(len(network.reluList)))
    print("Number of maxs: {}".format(len(network.maxList)))
    print("Number of disjunctions: {}".format(len(network.disjunctionList)))
    print("Number of equations: {}".format(len(network.equList)))
    print("Number of variables: {}".format(network.numVars))
    ipq = network.getMarabouQuery()
    MarabouCore.saveQuery(ipq, ipq_output)

def toMarabouEquation(equation):
    eq = MarabouCore.Equation(equation.EquationType)
    eq.setScalar(equation.scalar)
    for (c, x) in equation.addendList:
        eq.addAddend(c, x)
    return eq

def create_marabou_query(onnx_file, box_spec_list):
    pert_dim = 0
    input_spec, output_specs = box_spec_list[0]
    for i, (lb, ub) in enumerate(input_spec):
        if lb < ub:
            pert_dim += 1
    print(f"Perturbation dimension is {pert_dim}")

    network = MarabouNetworkONNX(onnx_file)
    outputVars = network.outputVars[0].flatten()
    if len(box_spec_list) == 1:
        input_spec, output_specs = box_spec_list[0]
        if len(output_specs) == 1:
            output_props, rhss = output_specs[0]
            for i in range(len(rhss)):
                eq = Equation(MarabouCore.Equation.LE)
                for out_index, c in output_props[i].items():
                    eq.addAddend(c, outputVars[out_index])
                eq.setScalar(rhss[i])
                network.addEquation(eq)
        elif len(output_specs) > 1:
            disjuncts = []
            for output_props, rhss in output_specs:
                conjuncts = []
                for i in range(len(rhss)):
                    eq = Equation(MarabouCore.Equation.LE)
                    for out_index, c in output_props[i].items():
                        eq.addAddend(c, outputVars[out_index])
                    eq.setScalar(rhss[i])
                    conjuncts.append(toMarabouEquation(eq))
                disjuncts.append(conjuncts)
            network.addDisjunctionConstraint(disjuncts)
    else:
        print("Unsupported input spec")
    return network


if __name__ == "__main__":
    # network vnnlib vnnlib.pickle ipq
    parse_vnnlib_file(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
