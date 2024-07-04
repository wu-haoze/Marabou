'''
Top contributors (to current version):
    - Kyle Julian
    - Haoze Wu
    - Teruhiro Tagomori
    - Tobey Shim
    - Idan Refaeli

This file is part of the Marabou project.
Copyright (c) 2017-2024 by the authors listed in the file AUTHORS
in the top-level source directory) and their institutional affiliations.
All rights reserved. See the file COPYING in the top-level source
directory for licensing information.

MarabouNetworkONNX represents neural networks with piecewise linear constraints derived from the ONNX format
'''
import onnx
import onnxruntime
from maraboupy.MarabouNetwork import MarabouNetwork
from maraboupy.parsers.ONNXParserThresh import ONNXParserThresh
import os

class MarabouNetworkONNXThresh(MarabouNetwork):
    """Constructs a MarabouNetworkONNX object from an ONNX file

    Args:
        filename (str): Path to the ONNX file
        inputNames: (list of str, optional): List of node names corresponding to inputs
        outputNames: (list of str, optional): List of node names corresponding to outputs

    Returns:
        :class:`~maraboupy.Marabou.marabouNetworkONNX.marabouNetworkONNX`
    """
    def __init__(self, filename, inputNames=None, outputNames=None,
                 equalityThreshold=30000, nonlinearityThreshold=30000,
                 candidateSubONNXFileName=None):
        super().__init__()
        self.readONNXThresh(filename, inputNames, outputNames,
                            equalityThreshold, nonlinearityThreshold, candidateSubONNXFileName=candidateSubONNXFileName)

    def readONNXThresh(self, filename, inputNames=None, outputNames=None, equalityThreshold=0, nonlinearityThreshold=0, preserveExistingConstraints=False, candidateSubONNXFileName=None):
        if not preserveExistingConstraints:
            self.clear()

        self.filename = filename

        self.graph = onnx.load(filename).graph

        # Setup input node names
        if inputNames is not None:
            # Check that input are in the graph
            for name in inputNames:
                if not len([nde for nde in self.graph.node if name in nde.input]):
                    raise RuntimeError("Input %s not found in graph!" % name)
            self.inputNames = inputNames
        else:
            # Get default inputs if no names are provided
            assert len(self.graph.input) >= 1
            initNames = [node.name for node in self.graph.initializer]
            self.inputNames = [inp.name for inp in self.graph.input if inp.name not in initNames]

        # Setup output node names
        if outputNames is not None:
            if isinstance(outputNames, str):
                outputNames = [outputNames]

            # Check that outputs are in the graph
            for name in outputNames:
                if not len([nde for nde in self.graph.node if name in nde.output]):
                    raise RuntimeError("Output %s not found in graph!" % name)
            self.outputNames = outputNames
        else:
            # Get all outputs if no names are provided
            assert len(self.graph.output) >= 1
            initNames = [node.name for node in self.graph.initializer]
            self.outputNames = [out.name for out in self.graph.output if out.name not in initNames]

        self.subONNXFile = ONNXParserThresh.parse(filename, self, self.graph, self.inputNames, self.outputNames, equalityThreshold, nonlinearityThreshold, candidateSubONNXFileName)

    def getNode(self, nodeName):
        """Find the node in the graph corresponding to the given name

        Args:
            nodeName (str): Name of node to find in graph

        Returns:
            (str): ONNX node named nodeName

        :meta private:
        """
        nodes = [node for node in self.graph.node if nodeName in node.output]
        assert len(nodes) == 1
        return nodes[0]

    def evaluateWithoutMarabou(self, inputValues):
        """Try to evaluate the network with the given inputs using ONNX

        Args:
            inputValues (list of numpy array): Input values representing inputs to network

        Returns:
            (list of numpy array): Output values of neural network
        """
        # Check that all input variables are designated as inputs in the graph
        # Unlike Tensorflow, ONNX only allows assignment of values to input/output nodes
        onnxInputNames = [node.name for node in self.graph.input]
        for inName in self.inputNames:
            if inName not in onnxInputNames:
                raise NotImplementedError("ONNX does not allow intermediate layers to be set as inputs!")

        # Check that the output variable is designated as an output in the graph
        # Unlike Tensorflow, ONNX only allows assignment of values to input/output nodes
        onnxOutputNames = [node.name for node in self.graph.output]
        for outputName in self.outputNames:
            if outputName not in onnxOutputNames:
                raise NotImplementedError("ONNX does not allow intermediate layers to be set as the output!")

        initNames =  [node.name for node in self.graph.initializer]
        graphInputs = [inp.name for inp in self.graph.input if inp.name not in initNames]
        if len(inputValues) != len(graphInputs):
            raise RuntimeError("There are %d inputs to network, but only %d input arrays were given."%(len(graphInputs), len(inputValues)))

        # Use onnxruntime session to evaluate the point
        sess = onnxruntime.InferenceSession(self.filename)
        input_dict = dict()
        for i, inputName in enumerate(self.inputNames):

            # Try to cast input to correct type
            onnxType = sess.get_inputs()[i].type
            if 'float' in onnxType:
                inputType = 'float32'
            else:
                raise NotImplementedError("Inputs to network expected to be of type 'float', not %s" % onnxType)
            input_dict[inputName] = inputValues[i].reshape(self.inputVars[i].shape).astype(inputType)
        return sess.run(self.outputNames, input_dict)
