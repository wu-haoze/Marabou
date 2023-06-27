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
from maraboupy.MarabouNetworkONNXThresh import *
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

    create_marabou_query(onnx_file, box_spec_list, ipq_output)

def toMarabouEquation(equation):
    eq = MarabouCore.Equation(equation.EquationType)
    eq.setScalar(equation.scalar)
    for (c, x) in equation.addendList:
        eq.addAddend(c, x)
    return eq

def create_marabou_query(onnx_file, box_spec_list, ipq_output):
    query_id = 1
    inputVarsMap = dict()
    outputVarsMap = dict()
    queriesMap = dict()

    network = MarabouNetworkONNXThresh(onnx_file)
    inputVars = network.inputVars[0].flatten()
    if len(box_spec_list) == 1:
        pert_dim = 0
        input_spec, _ = box_spec_list[0]
        for i, (lb, ub) in enumerate(input_spec):
            if lb < ub:
                pert_dim += 1
            network.setLowerBound(inputVars[i], lb)
            network.setUpperBound(inputVars[i], ub)
        print(f"Perturbation dimension is {pert_dim}")
    else:
        print("Unsupported input spec")
        exit(12)

    while network.subONNXFile is not None:
        queryName = f"{ipq_output}_{query_id}"
        network.saveQuery(queryName)
        inputVarsMap[query_id] = network.inputVars
        outputVarsMap[query_id] = network.outputVars
        queriesMap[query_id] = queryName

        onnxFile = network.subONNXFile
        del network
        network = MarabouNetworkONNXThresh(onnxFile)
        query_id += 1

    outputVars = network.outputVars[0].flatten()
    if len(box_spec_list) == 1:
        _, output_specs = box_spec_list[0]
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


    queryName = f"{ipq_output}_{query_id}"
    network.saveQuery(queryName)
    inputVarsMap[query_id] = network.inputVars
    outputVarsMap[query_id] = network.outputVars
    queriesMap[query_id] = queryName
    return


if __name__ == "__main__":
    # network vnnlib vnnlib.pickle ipq
    parse_vnnlib_file(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
