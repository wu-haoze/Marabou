# Supress warnings caused by tensorflow
import warnings
warnings.filterwarnings('ignore', category = DeprecationWarning)
warnings.filterwarnings('ignore', category = PendingDeprecationWarning)

import pytest
from .. import Marabou
import numpy as np
import os

# Global settings
OPT = Marabou.createOptions(verbosity = 0) # Turn off printing
TOL = 1e-4                                 # Set tolerance for checking Marabou evaluations
NETWORK_FOLDER = "../../resources/onnx/"   # Folder for test networks
np.random.seed(123)                        # Seed random numbers for repeatability
NUM_RAND = 10                              # Default number of random test points per example


def test_tanh():
    """
    Uses Add and MatMul layers with Tanh activation function.
    """
    filename =  "tanh_test.onnx"
    evaluateFile(filename)

def evaluateFile(filename, inputNames = None, outputNames = None, testInputs = None, numPoints = NUM_RAND):
    """
    Load network and evaluate testInputs with and without Marabou
    Args:
        filename (str): name of network file without path
        inputNames (list of str): name of input layers
        outputNames (list of str): name of output layers
        testInputs (list of arrays): test points to evaluate. Points created if none provided
        numPoints (int): Number of test points to create if testInputs is none
    """
    # Load network relative to this file's location
    filename = os.path.join(os.path.dirname(__file__), NETWORK_FOLDER, filename)
    network = Marabou.read_onnx(filename, inputNames = inputNames, outputNames = outputNames)
    
    # Create test points if none provided. This creates a list of test points.
    # Each test point is itself a list, representing the values for each input array.
    if not testInputs:
        testInputs = [[np.random.random(inVars.shape) for inVars in network.inputVars] for _ in range(numPoints)]
    
    # Evaluate test points using both Marabou and ONNX, and assert that the error is less than TOL
    for testInput in testInputs:
        err = network.findError(testInput, options=OPT, filename="")
        for i in range(len(err)):
            assert max(err[i].flatten()) < TOL
    
def evaluateIntermediateLayers(filename, inputNames = None, outputNames = None, intermediateNames = None, testInputs = None, numPoints = None):
    """
    This function loads three networks: the full network, the initial portion up to the intermediate layer, and 
    the final portion beginning with the intermediate layer. This function compares the Marabou networks to ONNX
    when possible, and checks that the two portions together give the same result as the full network.
    Args:
        filename (str): name of network file without path
        inputNames (list of str): name of input layers
        outputNames (list of str): name of output layers
        intermediateNames (list of str): name of intermediate layers. Must be added to graph.output in onnx model!
        testInputs (list of arrays): test points to evaluate. Points created if none provided
        numPoints (int): Number of test points to create if testInputs is none
    """
    filename = os.path.join(os.path.dirname(__file__), NETWORK_FOLDER, filename)
    fullNetwork = Marabou.read_onnx(filename, inputNames = inputNames, outputNames = outputNames)
    startNetwork = Marabou.read_onnx(filename, inputNames = inputNames, outputNames = intermediateNames)
    endNetwork = Marabou.read_onnx(filename, inputNames = intermediateNames, outputNames = outputNames)
    
    testInputs = [[np.random.random(inVars.shape) for inVars in fullNetwork.inputVars] for _ in range(NUM_RAND)]
    for testInput in testInputs:
        marabouEval_full = fullNetwork.evaluateWithMarabou(testInput, options = OPT, filename = "")
        onnxEval_full = fullNetwork.evaluateWithoutMarabou(testInput)
        assert len(marabouEval_full) == len(onnxEval_full)
        for i in range(len(marabouEval_full)):
            assert max(abs(marabouEval_full[i].flatten() - onnxEval_full[i].flatten())) < TOL
        
        marabouEval_start = startNetwork.evaluateWithMarabou(testInput, options = OPT, filename = "")
        onnxEval_start = startNetwork.evaluateWithoutMarabou(testInput)
        assert len(marabouEval_start) == len(onnxEval_start)
        for i in range(len(marabouEval_start)):
            assert max(abs(marabouEval_start[i].flatten() - onnxEval_start[i].flatten())) < TOL
        
        marabouEval_end = endNetwork.evaluateWithMarabou([marabouEval_start], options = OPT, filename = "")
        assert len(marabouEval_end) == len(onnxEval_full)
        for i in range(len(marabouEval_end)):
            assert max(abs(marabouEval_end[i].flatten() - onnxEval_full[i].flatten())) < TOL
