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
from maraboupy.Marabou import createOptions

def parse_vnnlib_file(onnx_file, vnnlib_file, pickle_output, ipq_output):
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

    print(os.path.basename(onnx_file))
    if "vgg16-7" not in os.path.basename(onnx_file) and "ml4acopf" not in os.path.basename(onnx_file):
        create_marabou_query(onnx_file, box_spec_list, ipq_output)

def toMarabouEquation(equation):
    eq = MarabouCore.Equation(equation.EquationType)
    eq.setScalar(equation.scalar)
    for (c, x) in equation.addendList:
        eq.addAddend(c, x)
    return eq

def test_network(network_object):
    testInputs = [np.random.random(inVars.shape) for inVars in network_object.inputVars]
    options = createOptions(tighteningStrategy="none")
    outputsMarabou = network_object.evaluateWithMarabou(testInputs, options=options)
    outputsONNX = network_object.evaluateWithoutMarabou(testInputs)
    print("###############Here #####################")
    print("Onnx:", list(outputsONNX[0][0]))
    #print("Marabou:", outputsMarabou[0])
    #print("Testing",  max(outputsMarabou[0][0] - outputsONNX[0][0]))

def create_marabou_query(onnx_file, box_spec_list, ipq_output):
    query_id = 1
    inputVarsMap = dict()
    outputVarsMap = dict()
    queriesMap = dict()

    if len(box_spec_list) > 1:
        print("Multiple input specs!")
        for box_spec in box_spec_list:
            network = MarabouNetworkONNX(onnx_file, reindexOutputVars=False)
            inputVars = network.inputVars[0].flatten()
            outputVars = network.outputVars[0].flatten()
            pert_dim = 0
            input_spec, output_specs = box_spec
            for i, (lb, ub) in enumerate(input_spec):
                if lb < ub:
                    pert_dim += 1
                network.setLowerBound(inputVars[i], lb)
                network.setUpperBound(inputVars[i], ub)
            print(f"Perturbation dimension is {pert_dim}")

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

            print("Number of disunctions:", len(network.disjunctionList))
            queryName = f"{ipq_output}_{query_id}"
            network.saveQuery(queryName)
            inputVarsMap[query_id] = network.inputVars
            queriesMap[query_id] = queryName
            del network
            query_id += 1

        print("Saving query info to", f"{ipq_output}.pickle")
        with open(f"{ipq_output}.pickle", 'wb') as handle:
            pickle.dump((query_id - 1, queriesMap, inputVarsMap, outputVarsMap), handle, protocol=pickle.HIGHEST_PROTOCOL)
    else:
        candidateSubONNXFileName=onnx_file[:-4] + f"part{query_id + 1}.onnx"
        network = MarabouNetworkONNXThresh(onnx_file,
                                           candidateSubONNXFileName=candidateSubONNXFileName)
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
            query_id += 1
            candidateSubONNXFileName=onnx_file[:-4] + f"part{query_id + 1}.onnx"
            network = MarabouNetworkONNXThresh(onnxFile,
                                               candidateSubONNXFileName=candidateSubONNXFileName)

        outputVars = network.outputVars[0].flatten()
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

        print("Number of disunctions:", len(network.disjunctionList))

        queryName = f"{ipq_output}_{query_id}"
        network.saveQuery(queryName)
        inputVarsMap[query_id] = network.inputVars
        outputVarsMap[query_id] = network.outputVars
        queriesMap[query_id] = queryName

        with open(f"{ipq_output}.pickle", 'wb') as handle:
            print("Saving query info to", f"{ipq_output}.pickle")
            pickle.dump((query_id, queriesMap, inputVarsMap, outputVarsMap), handle, protocol=pickle.HIGHEST_PROTOCOL)
    return


if __name__ == "__main__":
    # network vnnlib vnnlib.pickle ipq
    parse_vnnlib_file(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
