'''
Top contributors (to current version):
    - Kyle Julian
    - Haoze Wu
    - Teruhiro Tagomori
    - Tobey Shim

This file is part of the Marabou project.
Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
in the top-level source directory) and their institutional affiliations.
All rights reserved. See the file COPYING in the top-level source
directory for licensing information.

MarabouNetworkONNXThresh represents neural networks with piecewise linear constraints derived from the ONNX format
'''

import numpy as np
import onnx
import onnxruntime
from onnx import numpy_helper
import os
from onnx.helper import get_attribute_value
from maraboupy import MarabouUtils
from maraboupy import MarabouNetwork
from onnx import TensorProto
import itertools
import torch
import tempfile

class MarabouNetworkONNXThresh(MarabouNetwork.MarabouNetwork):

    def __init__(self, filename, inputNames=None, outputNames=None,
                 equalityThreshold=50000, nonlinearityThreshold=50000,
                 candidateSubONNXFileName=None):
        super().__init__()
        self.thresholdReached = False
        self.candidateSubONNXFileName = candidateSubONNXFileName
        self.subONNXFile = None

        self.readONNXThresh(filename, inputNames, outputNames,
                            equalityThreshold, nonlinearityThreshold)

    def clear(self):
        """Reset values to represent empty network
        """
        super().clear()
        self.madeGraphEquations = []
        self.varMap = dict()
        self.constantMap = dict()
        self.shapeMap = dict()
        self.inputNames = None
        self.outputNames = None
        self.graph = None

    def shallowClear(self):
        """Reset values to represent new copy
        of network while maintaining
        previous constraints. Used for
        unrolling system dynamics.
        """
        self.madeGraphEquations = []
        self.varMap = dict()
        self.constantMap = dict()
        self.shapeMap = dict()
        self.inputNames = None
        self.outputNames = None
        self.graph = None

    def readONNXThresh(self, filename, inputNames, outputNames,
                       equalityThreshold, nonlinearityThreshold):
        """Read an ONNX file and create a MarabouNetworkONNXThresh object

        Args:
            filename: (str): Path to the ONNX file
            inputNames: (list of str): List of node names corresponding to inputs
            outputNames: (list of str): List of node names corresponding to outputs
            reindexOutputVars: (bool): Reindex the variables so that the output variables are immediate after input variables.

        :meta private:
        """
        self.filename = filename

        self.equalityThreshold = equalityThreshold
        self.nonlinearityThreshold = nonlinearityThreshold

        self.graph = onnx.load(filename).graph

        # Get default inputs/outputs if no names are provided
        if not inputNames:
            assert len(self.graph.input) >= 1
            initNames = [node.name for node in self.graph.initializer]
            inputNames = [inp.name for inp in self.graph.input if inp.name not in initNames]
        if not outputNames:
            assert len(self.graph.output) >= 1
            initNames = [node.name for node in self.graph.initializer]
            outputNames = [out.name for out in self.graph.output if out.name not in initNames]
        elif isinstance(outputNames, str):
            outputNames = [outputNames]

        # Check that input/outputs are in the graph
        for name in inputNames:
            if not len([nde for nde in self.graph.node if name in nde.input]):
                raise RuntimeError("Input {} not found in graph!".format(name))
        for name in outputNames:
            if not len([nde for nde in self.graph.node if name in nde.output]):
                raise RuntimeError("Output {} not found in graph!".format(name))

        self.inputNames = inputNames
        self.outputNames = outputNames

        # Process the shapes and values of the graph while making Marabou equations and constraints
        self.foundnInputFlags = 0
        self.processGraph()

        self.outputVars = [self.varMap[outputName] for outputName in self.outputNames]

    def processGraph(self):
        """Processes the ONNX graph to produce Marabou equations

        :meta private:
        """
        # Add shapes for the graph's inputs
        for node in self.graph.input:
            self.shapeMap[node.name] = list([dim.dim_value if dim.dim_value > 0 else 1 for dim in node.type.tensor_type.shape.dim])

            # If we find one of the specified inputs, create new variables
            if node.name in self.inputNames:
                self.madeGraphEquations += [node.name]
                self.foundnInputFlags += 1
                self.makeNewVariables(node.name)
                self.inputVars += [np.array(self.varMap[node.name])]

        # Add shapes for constants
        for node in self.graph.initializer:
            self.shapeMap[node.name] = list(node.dims)
            self.madeGraphEquations += [node.name]

        # Recursively create remaining shapes and equations as needed
        for outputName in self.outputNames:
            self.makeGraphEquations(outputName, True)

    def makeGraphEquations(self, nodeName, makeEquations):
        """Recursively populates self.shapeMap, self.varMap, and self.constantMap while adding equations and constraints

        Args:
            nodeName (str): Name of node for making the shape
            makeEquations (bool): Create Marabou equations for this node if True

        :meta private:
        """
        if nodeName in self.madeGraphEquations:
            return

        if nodeName in self.inputNames:
            self.foundnInputFlags += 1
            # If an inputName is an intermediate layer of the network, we don't need to create Marabou
            # equations for its inputs. However, we still need to call makeMarabouEquations in order to
            # compute shapes. We just need to set the makeEquations flag to false
            makeEquations = False
        self.madeGraphEquations += [nodeName]

        # Recursively call makeGraphEquations, then call makeMarabouEquations
        # This ensures that shapes and values of a node's inputs have been computed first
        for inNodeName in self.getInputNodes(nodeName):
            self.makeGraphEquations(inNodeName, makeEquations)

        # By this point, all input variables need to have been found
        if self.foundnInputFlags != len(self.inputNames):
            err_msg = "These input variables could not be found: %s"%(", ".join([inVar for inVar in self.inputNames if inVar not in self.varMap]))
            raise RuntimeError(err_msg)

        if not self.thresholdReached:
            # Compute node's shape and create Marabou equations as needed
            self.makeMarabouEquations(nodeName, makeEquations)

            numEquations = len(self.equList)
            numNonLinearities = (len(self.reluList) + len(self.sigmoidList) +
                                 len(self.maxList) + len(self.absList) + len(self.signList))
            if (numEquations > self.equalityThreshold and
                numNonLinearities > self.nonlinearityThreshold):
                print(f"Split threshold reached: {numEquations} equations, {numNonLinearities} nonlinear constraints")
                if self.splitNetworkAtNode(nodeName):
                    self.thresholdReached = True

        # Create new variables when we find one of the inputs
        if nodeName in self.inputNames:
            self.makeNewVariables(nodeName)
            self.inputVars += [np.array(self.varMap[nodeName])]

    def splitNetworkAtNode(self, nodeName):
        print(f"Attempting to split the network at node {nodeName}...")
        assert(len(self.outputNames) == 1)
        currentNodeName = self.outputNames[0]
        noResidualAfter = True
        while currentNodeName != nodeName:
            inNodeNames = self.getInputNodes(currentNodeName)
            if len(inNodeNames) > 1:
                noResidualAfter = False
                break
            else:
                currentNodeName = inNodeNames[0]

        assert(len(self.inputNames) == 1)
        currentNodeName = nodeName
        noResidualBefore = True
        while currentNodeName != self.inputNames[0]:
            inNodeNames = self.getInputNodes(currentNodeName)
            numVariableNode = 0
            for inNodeName in inNodeNames:
                if inNodeName in self.constantMap:
                    continue
                else:
                    numVariableNode += 1
            if numVariableNode > 1:
                noResidualBefore = False
                break
            elif numVariableNode == 1:
                currentNodeName = inNodeNames[0]
            else:
                break

        print(f"No residual after: {noResidualAfter}, no residual before: {noResidualBefore}")
        if noResidualAfter or noResidualBefore:
            outputName = self.getNode(nodeName).output[0]
            self.subONNXFile = self.candidateSubONNXFileName
            onnx.utils.extract_model(self.filename, self.subONNXFile,
                                     input_names=[outputName],
                                     output_names=self.outputNames)
            self.outputNames = [outputName]
            print(f"Attempting to split the network at node {nodeName} - successful!")
            return True
        else:
            return False


    def makeMarabouEquations(self, nodeName, makeEquations):
        """Compute the shape and values of a node assuming the input shapes and values have been computed already.

        Args:
            nodeName (str): Name of node for which we want to compute the output shape
            makeEquations (bool): Create Marabou equations for this node if True
            
        :meta private:
        """
        node = self.getNode(nodeName)
        print(node.op_type)
        if node.op_type == 'Shape':
            self.shape_(node)
        elif node.op_type == 'Dropout':
            self.identity(node)
        elif node.op_type == 'Constant':
            self.constant(node)
        elif node.op_type == 'ConstantOfShape':
            self.constantOfShape(node)
        elif node.op_type == 'Identity': 
            self.identity(node)
        elif node.op_type == 'Cast':
            self.cast(node)
        elif node.op_type == 'Reshape':
            self.reshape(node)
        elif node.op_type == 'Flatten':
            self.flatten(node)
        elif node.op_type == "Transpose":
            self.transpose(node)
        elif node.op_type == 'Slice':
            self.slice(node)
        elif node.op_type == 'Gather':
            self.gather(node)
        elif node.op_type == 'Unsqueeze':
            self.unsqueeze(node)
        elif node.op_type == 'Upsample':
            self.upsample(node)
        elif node.op_type == "BatchNormalization":
            self.batchNorm(node, makeEquations)
        elif node.op_type == 'Concat':
            self.concatEquations(node)
        elif node.op_type == "MaxPool":
            self.maxpoolEquations(node, makeEquations)
        elif node.op_type == "ConvTranspose":
            self.convTransposeEquations(node, makeEquations)
        elif node.op_type == "Conv":
            self.convEquations(node, makeEquations)
        elif node.op_type == 'Gemm':
            self.gemmEquations(node, makeEquations)
        elif node.op_type == 'MatMul':
            self.matMulEquations(node, makeEquations)
        elif node.op_type == 'Mul':
            self.mulEquations(node, makeEquations)
        elif node.op_type == 'Add':
            self.addEquations(node, makeEquations)
        elif node.op_type == 'Sub':
            self.subEquations(node, makeEquations)
        elif node.op_type == 'Pow':
            self.powEquations(node, makeEquations)
        elif node.op_type == 'Neg':
            self.negEquations(node, makeEquations)
        elif node.op_type == 'Div':
            self.divEquations(node, makeEquations)
        elif node.op_type == 'Cos':
            self.cosEquations(node, makeEquations)
        elif node.op_type == 'Sin':
            self.sinEquations(node, makeEquations)
        elif node.op_type == 'Relu': 
            self.reluEquations(node, makeEquations)
        elif node.op_type == 'Sigmoid':
            self.sigmoidEquations(node, makeEquations)
        elif node.op_type == 'Sign':
            self.signEquations(node, makeEquations)
        elif node.op_type == 'Split':
            self.splitEquations(node, nodeName, makeEquations)
        elif node.op_type == 'Resize':
            self.resizeEquations(node, makeEquations)
        elif node.op_type == 'Tanh':
            self.tanhEquations(node, makeEquations)
        elif node.op_type == 'Softmax':
            self.softmaxEquations(node, makeEquations)
        elif node.op_type == 'ReduceMean':
            self.reduceMeanEquations(node, makeEquations)
        else:
            raise NotImplementedError("Operation {} not implemented".format(node.op_type))
    
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
    
    def makeNewVariables(self, nodeName):
        """Assuming the node's shape is known, return a set of new variables in the same shape

        Args:
            nodeName (str): Name of node

        Returns:
            (numpy array): Array of variable numbers 

        :meta private:
        """
        assert nodeName not in self.varMap
        shape = self.shapeMap[nodeName]
        size = np.prod(shape)
        v = np.array([self.getNewVariable() for _ in range(size)]).reshape(shape)
        self.varMap[nodeName] = v
        assert all([np.equal(np.mod(i, 1), 0) for i in v.reshape(-1)]) # check if integers
        return v
        
    def getInputNodes(self, nodeName):
        """Get names of nodes that are inputs to the given node

        Args:
            nodeName (str): Name of node

        Returns:
            (list of str): Names of nodes that are inputs to the given node

        :meta private:
        """
        node = self.getNode(nodeName)
        inNodes = []
        for inp in node.input:
            if len([nde for nde in self.graph.node if inp in nde.output]):
                inNodes += [inp]
            elif len([nde for nde in self.graph.initializer if nde.name == inp]):
                self.constantMap[inp] = [numpy_helper.to_array(init) for init in self.graph.initializer if init.name == inp][0]
        return inNodes
        
    def constant(self, node):
        """Function representing a constant tensor

        Args:
            node (node): ONNX node representing constant operation

        :meta private:
        """
        nodeName = node.output[0]
        for attr in node.attribute:
            if attr.name == "value":
                self.constantMap[nodeName] = numpy_helper.to_array(get_attribute_value(attr))
                return
        raise RuntimeError("Could not find value of tensor constant")

    def constantOfShape(self, node):
        """Function representing a constant tensor

        Args:
            node (node): ONNX node representing constant operation

        :meta private:
        """
        nodeName = node.output[0]
        inputName = node.input[0]

        value = 0
        for attr in node.attribute:
            if attr.name == "value":
                value = numpy_helper.to_array(get_attribute_value(attr))

        shape = self.constantMap[inputName]
        self.shapeMap[nodeName] = shape
        self.constantMap[nodeName] = np.ones(shape) * value

        return

    def shape_(self, node):
        """Function representing a shape tensor

        Args:
            node (node): ONNX node representing shape operation

        :meta private:
        """
        nodeName = node.output[0]
        inputName = node.input[0]

        shape = self.shapeMap[inputName]
        self.constantMap[nodeName] = shape
        self.shapeMap[nodeName] = [len(shape)]

    def identity(self, node):
        """Function representing identity

        Args:
            node (node): ONNX node representing identity operation

        :meta private:
        """
        nodeName = node.output[0]
        inputName = node.input[0]
        self.shapeMap[nodeName] = self.shapeMap[inputName]
        if inputName in self.varMap:
            self.varMap[nodeName] = self.varMap[inputName]
        elif inputName in self.constantMap:
            self.constantMap[nodeName] = self.constantMap[inputName]
            
    def cast(self, node):
        """Function representing cast

        Args:
            node (node): ONNX node representing cast operation

        :meta private:
        """
        nodeName = node.output[0]
        inputName = node.input[0]
        self.shapeMap[nodeName] = self.shapeMap[inputName]
        
        # Try to find type to cast to. If not found, raise error
        to = None
        for attr in node.attribute:
            if attr.name == "to":
                to = get_attribute_value(attr)
        if to is None:
            raise RuntimeError("Casting type not specified with attribute 'to'")
            
        # Cast input array to correct type, and throw error if type is unknown
        if inputName in self.constantMap:
            if to == TensorProto.FLOAT16:
                self.constantMap[nodeName] = self.constantMap[inputName].astype('float16')
            elif to == TensorProto.FLOAT:
                self.constantMap[nodeName] = self.constantMap[inputName].astype('float32')
            elif to == TensorProto.DOUBLE:
                self.constantMap[nodeName] = self.constantMap[inputName].astype('double')
            elif to == TensorProto.UINT8:
                self.constantMap[nodeName] = self.constantMap[inputName].astype('uint8')
            elif to == TensorProto.UINT16:
                self.constantMap[nodeName] = self.constantMap[inputName].astype('uint16')
            elif to == TensorProto.UINT32:
                self.constantMap[nodeName] = self.constantMap[inputName].astype('uint32')
            elif to == TensorProto.UINT64:
                self.constantMap[nodeName] = self.constantMap[inputName].astype('uint64')
            elif to == TensorProto.INT8:
                self.constantMap[nodeName] = self.constantMap[inputName].astype('int8')
            elif to == TensorProto.INT16:
                self.constantMap[nodeName] = self.constantMap[inputName].astype('int16')
            elif to == TensorProto.INT32:
                self.constantMap[nodeName] = self.constantMap[inputName].astype('int32')
            elif to == TensorProto.INT64:
                self.constantMap[nodeName] = self.constantMap[inputName].astype('int64')
            else:
                err_msg = "Unknown type for casting: %d\n" % to
                err_msg += "Check here for ONNX TensorProto: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto"
                raise NotImplementedError(err_msg)
                
        # We shouldn't be casting variables to different types, since Marabou assumes variables have double precision
        elif inputName in self.varMap:
            raise NotImplementedError("Casting variables not allowed with Marabou")

    def reshape(self, node):
        """Function representing reshape

        Args:
            node (node): ONNX node representing reshape operation

        :meta private:
        """
        nodeName = node.output[0]
        inputName1, inputName2 = node.input
        
        # Assume first input is array to be reshaped, second input is the new shape array
        reshapeVals = self.constantMap[inputName2]
        self.shapeMap[nodeName] = list(np.zeros(self.shapeMap[inputName1]).reshape(reshapeVals).shape)
        if inputName1 in self.varMap:
            self.varMap[nodeName] = self.varMap[inputName1].reshape(reshapeVals)
        elif inputName1 in self.constantMap:
            self.constantMap[nodeName] = self.constantMap[inputName1].reshape(reshapeVals)

    def flatten(self, node):
        """Function representing flatten

        Unlike numpy.flatten(), ONNX's Flatten operation reshapes
        a (d_0, d_1, ..., d_n) tensor into a 2D tensor with shape
        (d_0 * d_1 * ... * d_(axis-1), d_axis * d_(axis+1) * ... * d_n).

        Args:
            node (node): ONNX node representing flatten operation

        :meta private:
        """
        nodeName = node.output[0]

        # Assume first input is array to be flattened
        inputName = node.input[0]
        axis = 1
        for attr in node.attribute:
            if attr.name == "axis":
                axis = get_attribute_value(attr)

        dimension1 = int(np.prod(self.shapeMap[inputName][:axis]))
        dimension2 = int(np.prod(self.shapeMap[inputName][axis:]))
        newShape = [dimension1, dimension2]
        self.shapeMap[nodeName] = newShape

        if inputName in self.varMap:
            self.varMap[nodeName] = self.varMap[inputName].reshape(newShape)
        elif inputName in self.constantMap:
            self.constantMap[nodeName] = self.constantMap[inputName].reshape(newShape)
            
    def transpose(self, node):
        """Function representing transpose

        Args:
            node (node): ONNX node representing transpose operation

        :meta private:
        """
        nodeName = node.output[0]
        inputName = node.input[0]

        # Get attributes
        perm = None
        for attr in node.attribute:
            if attr.name == "perm":
                perm = get_attribute_value(attr)
        if perm is None:
            raise RuntimeError("Permutation indices not specified by attibute 'perm'")

        if len(perm) == len(self.shapeMap[inputName]) + 1:
            if perm[0] == 0:
                perm = [p -1 for p in perm[1:]]
            else:
                self.varMap[inputName] = np.array([self.varMap[inputName]])
                self.shapeMap[inputName] = self.varMap[inputName].shape

        self.shapeMap[nodeName] = [self.shapeMap[inputName][p] for p in perm]
        if inputName in self.varMap:
            self.varMap[nodeName] = \
            np.transpose(self.varMap[node.input[0]].reshape(self.shapeMap[node.input[0]]),
                         perm)
        elif inputName in self.constantMap:
            self.constantMap[nodeName] = np.transpose(self.constantMap[inputName], perm)

    def slice(self, node):
        """Function representing slice

        Args:
            node (node): ONNX node representing slice operation

        :meta private:
        """
        nodeName = node.output[0]
        inputName = node.input[0]
        # create a temporary file in the current directory
        tf = tempfile.NamedTemporaryFile(dir=".")

        # get the file name
        temp_file_name = tf.name

        # close the file
        tf.close()
        onnx.utils.extract_model(self.filename, temp_file_name,
                                 input_names=[inputName],
                                 output_names=[nodeName])
        session = onnxruntime.InferenceSession(temp_file_name)
        os.remove(temp_file_name)

        # get the input and output names
        input_name = session.get_inputs()[0].name
        output_name = session.get_outputs()[0].name

        if inputName in self.varMap:
            # prepare the input data
            input_data = np.array(self.varMap[inputName], dtype="float32")
            # run the inference session and get the output predictions
            output_data = session.run([output_name], {input_name: input_data})[0].astype(int)
            self.shapeMap[nodeName] = output_data.shape
            self.varMap[nodeName] = output_data
        else:
            # prepare the input data
            input_data = np.array(self.constantMap[inputName], dtype=int)
            # run the inference session and get the output predictions
            output_data = session.run([output_name], {input_name: input_data})[0].astype(int)
            self.shapeMap[nodeName] = output_data.shape
            self.constantMap[nodeName] = output_data

    def gather(self, node):
        """Function representing gather

        Args:
            node (node): ONNX node representing gather operation

        :meta private:
        """
        nodeName = node.output[0]
        inputName = node.input[0]
        # create a temporary file in the current directory
        tf = tempfile.NamedTemporaryFile(dir=".")

        # get the file name
        temp_file_name = tf.name

        # close the file
        tf.close()
        onnx.utils.extract_model(self.filename, temp_file_name,
                                 input_names=[inputName],
                                 output_names=[nodeName])
        session = onnxruntime.InferenceSession(temp_file_name)
        os.remove(temp_file_name)

        # get the input and output names
        input_name = session.get_inputs()[0].name
        output_name = session.get_outputs()[0].name

        if inputName in self.varMap:
            # prepare the input data
            input_data = np.array(self.varMap[inputName], dtype="float32")
            # run the inference session and get the output predictions
            output_data = session.run([output_name], {input_name: input_data})[0].astype(int)
            self.shapeMap[nodeName] = output_data.shape
            self.varMap[nodeName] = output_data
        else:
            # prepare the input data
            input_data = np.array(self.constantMap[inputName], dtype=int)
            # run the inference session and get the output predictions
            output_data = session.run([output_name], {input_name: input_data})[0].astype(int)
            self.shapeMap[nodeName] = output_data.shape
            self.constantMap[nodeName] = output_data

    def unsqueeze(self, node):
        """Function representing gather

        Args:
            node (node): ONNX node representing gather operation

        :meta private:
        """
        nodeName = node.output[0]
        inputName = node.input[0]
        # create a temporary file in the current directory
        tf = tempfile.NamedTemporaryFile(dir=".")

        # get the file name
        temp_file_name = tf.name

        # close the file
        tf.close()
        onnx.utils.extract_model(self.filename, temp_file_name,
                                 input_names=[inputName],
                                 output_names=[nodeName])
        session = onnxruntime.InferenceSession(temp_file_name)
        os.remove(temp_file_name)

        # get the input and output names
        input_name = session.get_inputs()[0].name
        output_name = session.get_outputs()[0].name

        if inputName in self.varMap:
            # prepare the input data
            input_data = np.array(self.varMap[inputName], dtype="float32")
            # run the inference session and get the output predictions
            output_data = session.run([output_name], {input_name: input_data})[0].astype(int)
            self.shapeMap[nodeName] = output_data.shape
            self.varMap[nodeName] = output_data
        else:
            # prepare the input data
            input_data = np.array(self.constantMap[inputName], dtype="int")
            # run the inference session and get the output predictions
            output_data = session.run([output_name], {input_name: input_data})[0].astype(int)
            self.shapeMap[nodeName] = output_data.shape
            self.constantMap[nodeName] = output_data


    def upsample(self, node):
        """Function to generate equations corresponding to upsample

        Args:
            node (node): ONNX node representing the Resize operation
            makeEquations (bool): True if we need to create new variables and write Marabou equations

        :meta private:
        """
        nodeName = node.output[0]
        inputName, scaleName = node.input[0], node.input[1]

        # Check number of dimension of input
        inputVars = self.varMap[inputName]
        inputShape = inputVars.shape
        assert(inputVars.ndim == 4)

        # Get and check attributes
        mode = None

        for attr in node.attribute:
            if attr.name == "mode":
                mode = get_attribute_value(attr)
        assert(mode is None or mode == b"nearest")

        # Get scales
        scales = self.constantMap[scaleName]
        assert(len(scales) == inputVars.ndim)

        # Set output shape
        outputShape = (int(inputShape[0] * scales[0]), int(inputShape[1] * scales[1]),
                       int(inputShape[2] * scales[2]), int(inputShape[3] * scales[3]))
        self.shapeMap[nodeName] = outputShape

        # Get variables
        outVars = inputVars
        for i in range(4):
            outVars = np.repeat(outVars, scales[i], i)
        assert(outVars.shape == outputShape)
        self.varMap[nodeName] = outVars


    def batchNorm(self, node, makeEquations):
        """Function to generate equations for a BatchNormalization

        Args:
            node (node): ONNX node representing the BatchNormalization operation

        :meta private
        """

        nodeName = node.output[0]
        inputName = node.input[0]
        self.shapeMap[nodeName] = self.shapeMap[inputName]

        # Get attributes
        epsilon = None
        for attr in node.attribute:
            if attr.name == "epsilon":
                epsilon = get_attribute_value(attr)

        # Get inputs
        scales = self.constantMap[node.input[1]].reshape(-1)
        biases = self.constantMap[node.input[2]].reshape(-1)
        input_means = self.constantMap[node.input[3]].reshape(-1)
        input_variances = self.constantMap[node.input[4]].reshape(-1)

        if not makeEquations:
            return

        numChannels = len(scales)

        # Get variables
        inputVars = self.varMap[inputName].reshape(numChannels, -1)
        outputVars = self.makeNewVariables(nodeName).reshape(numChannels, -1)
        assert(inputVars.shape == outputVars.shape)

        numInputs = inputVars.shape[1]

        for i in range(numChannels):
            for j in range(numInputs):
                # Add equation
                # To know this computation,
                # refer to https://github.com/onnx/onnx/blob/master/docs/Operators.md#batchnormalization.
                e = MarabouUtils.Equation()
                e.addAddend(1 / np.sqrt(input_variances[i] + epsilon) * scales[i], inputVars[i][j])
                e.addAddend(-1, outputVars[i][j])
                e.setScalar(input_means[i] / np.sqrt(input_variances[i] + epsilon) * scales[i] - biases[i])
                self.addEquation(e)

    def maxpoolEquations(self, node, makeEquations):
        """Function to generate maxpooling equations

        Args:
            node (node): ONNX node representing maxpool operation
            makeEquations (bool): True if we need to create new variables and maxpool constraints

        :meta private:
        """
        nodeName = node.output[0]

        # Extract attributes and define shape
        inputShape = self.shapeMap[node.input[0]]
        kernel_shape = [1, 1]
        strides = [1, 1]
        for attr in node.attribute:
            if attr.name == 'kernel_shape':
                kernel_shape = get_attribute_value(attr)
            elif attr.name == 'strides':
                strides = get_attribute_value(attr)

        outputShape = [dim for dim in inputShape]
        outputShape[2] = int(np.ceil((inputShape[2] - ((kernel_shape[0] - 1) + 1) + 1) / strides[0]))
        outputShape[3] = int(np.ceil((inputShape[3] - ((kernel_shape[1] - 1) + 1) + 1) / strides[1]))
        self.shapeMap[nodeName] = outputShape

        if not makeEquations:
            return

        inVars = self.varMap[node.input[0]]
        outVars = self.makeNewVariables(nodeName)
        for i in range(outputShape[2]):
            for j in range(outputShape[3]):
                for k in range(outputShape[1]):
                    maxVars = set()
                    for di in range(strides[0]*i, strides[0]*i + kernel_shape[0]):
                        for dj in range(strides[1]*j, strides[1]*j + kernel_shape[1]):
                            if di < inputShape[2] and dj < inputShape[3]:
                                maxVars.add(inVars[0][k][di][dj])
                    self.addMaxConstraint(maxVars, outVars[0][k][i][j])

    def softmaxEquations(self, node, makeEquations):
        """Function to generate softmax equations

        Args:
        node (node): ONNX node representing maxpool operation
        makeEquations (bool): True if we need to create new variables and maxpool constraints

        :meta private:
        """
        nodeName = node.output[0]

        # Extract attributes and define shape
        inputShape = self.shapeMap[node.input[0]]
        axis = -1
        for attr in node.attribute:
            if attr.name == 'axis':
                axis = get_attribute_value(attr)

        self.shapeMap[nodeName] = inputShape

        if not makeEquations:
            return

        inVars = self.varMap[node.input[0]]
        outVars = self.makeNewVariables(nodeName)

        if len(inputShape) == 2 and inputShape[0] == 1:
            self.addSoftmaxConstraint(list(np.array(inVars).flatten()), list(np.array(outVars).flatten()))
        else:
            if axis > len(inputShape) - 1:
                if axis == len(inputShape):
                    axis -= 1
            else:
                axis = ( len(inputShape) + axis ) % len(inputShape)
            perm = []
            for i, s in enumerate(inputShape):
                if i == axis:
                    continue
                perm.append(i)
            perm.append(axis)

            inVarsReshaped = np.transpose(inVars, perm).reshape(-1, inputShape[axis])
            outVarsReshaped = np.transpose(outVars, perm).reshape(-1, inputShape[axis])
            for i in range(inVarsReshaped.shape[0]):
                self.addSoftmaxConstraint(inVarsReshaped[i], outVarsReshaped[i])

    def reduceMeanEquations(self, node, makeEquations):
        """Function to generate softmax equations

        Args:
        node (node): ONNX node representing maxpool operation
        makeEquations (bool): True if we need to create new variables and maxpool constraints

        :meta private:
        """
        nodeName = node.output[0]
        inputName = node.input[0]
        for attr in node.attribute:
            if attr.name == 'axes':
                axes = get_attribute_value(attr)
            elif attr.name == 'keepdims':
                keepdims = get_attribute_value(attr)
        assert(keepdims == 0)
        assert(axes == [1])
        axes = axes[0]

        inshape = self.shapeMap[inputName]
        data = np.zeros(inshape)
        outshape = np.mean(data, axis=axes, keepdims=(keepdims == 1)).shape
        self.shapeMap[nodeName] = outshape

        outVars = self.makeNewVariables(nodeName)
        inVars = self.varMap[inputName].reshape(inshape)

        if not makeEquations:
            return

        assert(len(inshape) == 3 and len(outshape) == 2)
        for i in range(inshape[0]):
            for j in range(outshape[1]):
                e = MarabouUtils.Equation()
                for k in range(inshape[1]):
                    e.addAddend(1 / inshape[1], inVars[i][k][j])
                e.addAddend(-1, outVars[i][j])
                e.setScalar(0)
                self.addEquation(e)
        return

    def insert_value(self, arr, value, z_width, z_height):
        newWidth = (arr.shape[0] - 1) * z_width + arr.shape[0]
        newHeight = (arr.shape[1] - 1) * z_height + arr.shape[1]
        resultArr = (np.ones((newWidth, newHeight)) * value).astype(int)
        for i in range(newWidth):
            for j in range(newHeight):
                if i % (1 + z_width) == 0 and j % (1 + z_height) == 0:
                    resultArr[i][j] = arr[int(i / (1 + z_width))][int(j / (1 + z_height))]
        return resultArr

    def convTransposeEquations(self, node, makeEquations):
        """Function to generate equations for a 2D convolution

        Args:
            node (node): ONNX node representing the 2D Convolution operation
            makeEquations (bool): True if we need to create new variables and write Marabou equations

        :meta private:
        """
        nodeName = node.output[0]

        # Extract information about convolution
        for attr in node.attribute:
            auto_pad = None
            if attr.name == 'strides':
                strides = get_attribute_value(attr)
            elif attr.name == "auto_pad":
                auto_pad = get_attribute_value(attr)
            elif attr.name == "kernel_shape":
                filter_width, filter_height = get_attribute_value(attr)
            elif attr.name == 'pads':
                pad_left, pad_bottom, pad_right, pad_top = get_attribute_value(attr)

        assert(auto_pad is None or auto_pad == b'NOTSET')
        #assert(pad_left == 0 and pad_bottom == 0 and pad_right == 0 and pad_top == 0)

        # Get input shape information
        # First input should be variable tensor, the second a weight matrix defining filters
        shape0 = self.shapeMap[node.input[0]]
        shape1 = self.shapeMap[node.input[1]]
        input_channels = shape0[1]
        input_width = shape0[2]
        input_height = shape0[3]
        num_filters = shape1[1]
        filter_channels = shape1[0]

        # The number of channels should match between input variable and filters
        assert input_channels == filter_channels

        # Compute output shape
        out_width = (input_width - 1) * strides[0] + filter_width #- pad_left - pad_right
        out_height = (input_height - 1) * strides[1] + filter_height #- pad_bottom - pad_top
        out_channels = num_filters
        self.shapeMap[nodeName] = [shape0[0], out_channels, out_width, out_height]
        if not makeEquations:
            return

        inVars = self.varMap[node.input[0]]
        weights = self.constantMap[node.input[1]]

        # The third input is optional and specifies a bias for each filter
        # Bias is 0 if third input is not given
        biases = np.zeros(num_filters)
        if len(node.input) == 3:
            biases = self.constantMap[node.input[2]]

        outVars = self.makeNewVariables(nodeName)

        for k in range(out_channels): # Out_channel corresponds to filter number
            indexToAddends = dict()
            for i in range(out_width):
                for j in range(out_height):
                    indexToAddends[(i,j)] = []

            for i in range(input_width):
                for j in range(input_height):
                    for di in range(filter_width):
                        for dj in range(filter_height):
                            w_ind = int(strides[0] * i+di)
                            h_ind = int(strides[1] * j+dj)
                            for dk in range(filter_channels):
                                var = inVars[0][dk][i][j]
                                c = weights[dk][k][di][dj]
                                indexToAddends[(w_ind, h_ind)].append((c, var))

            for i in range(out_width):
                for j in range(out_height):
                    e = MarabouUtils.Equation()
                    for c, v in indexToAddends[(i,j)]:
                        e.addAddend(c, v)
                    e.addAddend(-1, outVars[0][k][i][j])
                    e.setScalar(-biases[k])
                    self.addEquation(e)

        if pad_left > 0 and pad_top > 0:
            out_width -= (pad_left + pad_right)
            out_height -= (pad_bottom + pad_top)
            self.shapeMap[nodeName] = [shape0[0], out_channels, out_width, out_height]
            outVars = outVars[:,:,pad_left:-pad_right,pad_top:-pad_bottom]
            self.varMap[nodeName] = outVars


    def convEquations(self, node, makeEquations):
        """Function to generate equations for a 2D convolution

        Args:
            node (node): ONNX node representing the 2D Convolution operation
            makeEquations (bool): True if we need to create new variables and write Marabou equations

        :meta private:
        """
        nodeName = node.output[0]
        
        # Extract information about convolution
        strides = [1, 1]
        pads = [0, 0, 0, 0]
        for attr in node.attribute:
            if attr.name == 'strides':
                strides = get_attribute_value(attr)
            elif attr.name == 'pads':
                pads = get_attribute_value(attr)
        if len(pads) == 2:
            pad_left, pad_right = pads

            # Get input shape information
            # First input should be variable tensor, the second a weight matrix defining filters
            shape0 = self.shapeMap[node.input[0]]
            shape1 = self.shapeMap[node.input[1]]
            input_channels = shape0[1]
            input_width = shape0[2]
            num_filters = shape1[0]
            filter_channels = shape1[1]
            filter_width = shape1[2]

            # The third input is optional and specifies a bias for each filter
            # Bias is 0 if third input is not given
            biases = np.zeros(num_filters)
            if len(node.input) == 3:
                biases = self.constantMap[node.input[2]]

            # The number of channels should match between input variable and filters
            assert input_channels == filter_channels

            # Compute output shape
            out_width = (input_width - filter_width + pad_left + pad_right) // strides[0] + 1
            out_channels = num_filters
            self.shapeMap[nodeName] = [shape0[0], out_channels, out_width]

            if not makeEquations:
                return

            inVars = self.varMap[node.input[0]]
            weights = self.constantMap[node.input[1]]
            outVars = self.makeNewVariables(nodeName)

            ### Generate actual equations ###
            # There is one equation for every output variable
            for i in range(out_width):
                for k in range(out_channels): # Out_channel corresponds to filter number
                    e = MarabouUtils.Equation()

                    # The equation convolves the filter with the specified input region
                    # Iterate over the filter
                    for di in range(filter_width):
                        for dk in range(filter_channels):
                            w_ind = int(strides[0]*i+di - pad_left)
                            if w_ind < input_width and w_ind >= 0:
                                var = inVars[0][dk][w_ind]
                                c = weights[k][dk][di]
                                e.addAddend(c, var)

                    # Add output variable
                    e.addAddend(-1, outVars[0][k][i])
                    e.setScalar(-biases[k])
                    self.addEquation(e)
        else:
            pad_left, pad_bottom, pad_right, pad_top = pads

            # Get input shape information
            # First input should be variable tensor, the second a weight matrix defining filters
            shape0 = self.shapeMap[node.input[0]]
            shape1 = self.shapeMap[node.input[1]]
            input_channels = shape0[1]
            input_width = shape0[2]
            input_height = shape0[3]
            num_filters = shape1[0]
            filter_channels = shape1[1]
            filter_width = shape1[2]
            filter_height = shape1[3]

            # The third input is optional and specifies a bias for each filter
            # Bias is 0 if third input is not given
            biases = np.zeros(num_filters)
            if len(node.input) == 3:
                biases = self.constantMap[node.input[2]]

            # The number of channels should match between input variable and filters
            assert input_channels == filter_channels

            # Compute output shape
            out_width = (input_width - filter_width + pad_left + pad_right) // strides[0] + 1
            out_height = (input_height - filter_height + pad_bottom + pad_top) // strides[1] + 1
            out_channels = num_filters
            self.shapeMap[nodeName] = [shape0[0], out_channels, out_width, out_height]

            if not makeEquations:
                return

            inVars = self.varMap[node.input[0]]
            weights = self.constantMap[node.input[1]]
            outVars = self.makeNewVariables(nodeName)

            ### Generate actual equations ###
            # There is one equation for every output variable
            for i in range(out_width):
                for j in range(out_height):
                    for k in range(out_channels): # Out_channel corresponds to filter number
                        e = MarabouUtils.Equation()

                        # The equation convolves the filter with the specified input region
                        # Iterate over the filter
                        for di in range(filter_width):
                            for dj in range(filter_height):
                                for dk in range(filter_channels):
                                    w_ind = int(strides[0]*i+di - pad_left)
                                    h_ind = int(strides[1]*j+dj - pad_bottom)
                                    if h_ind < input_height and h_ind >= 0 and w_ind < input_width and w_ind >= 0:
                                        var = inVars[0][dk][w_ind][h_ind]
                                        c = weights[k][dk][di][dj]
                                        e.addAddend(c, var)

                        # Add output variable
                        e.addAddend(-1, outVars[0][k][i][j])
                        e.setScalar(-biases[k])
                        self.addEquation(e)
        
    def gemmEquations(self, node, makeEquations):  
        """Function to generate equations corresponding to Gemm (general matrix multiplication)

        Args:
            node (node): ONNX node representing the Gemm operation
            makeEquations (bool): True if we need to create new variables and write Marabou equations

        :meta private:
        """  
        nodeName = node.output[0]
        
        # Get inputs
        inputName1, inputName2, inputName3 = node.input
        shape1 = self.shapeMap[inputName1]
        shape2 = self.shapeMap[inputName2]
        
        # Transpose first two inputs if needed,
        # and save scaling parameters alpha and beta if set
        alpha = 1.0
        beta = 1.0
        transA = 0
        transB = 0
        for attr in node.attribute:
            if attr.name == 'transA':
                transA = get_attribute_value(attr)
            elif attr.name == 'transB':
                transB = get_attribute_value(attr)
            elif attr.name == 'alpha':
                alpha = get_attribute_value(attr)
            elif attr.name == 'beta':
                beta = get_attribute_value(attr)
        
        if transA:
            shape1 = shape1[::-1]
        if transB:
            shape2 = shape2[::-1]
        outShape = [shape1[0], shape2[1]]
        self.shapeMap[nodeName] = outShape
        if not makeEquations:
            return
        
        # Assume that first input is variables, second is Matrix for MatMul, and third is bias addition
        input1 = self.varMap[inputName1]
        input2 = self.constantMap[inputName2]
        input3 = self.constantMap[inputName3]
        
        # Transpose inputs
        if transA:
            input1 = np.transpose(input1)
        if transB:
            input2 = np.transpose(input2)
        input3 = np.broadcast_to(input3, outShape)
        
        assert shape1[-1] == shape2[0]
        assert shape1[0] == outShape[0]
        assert shape2[1] == outShape[1]

        # Create new variables
        outputVariables = self.makeNewVariables(nodeName)
        # Generate equations
        for i in range(shape1[0]):
            for j in range(shape2[1]):
                e = MarabouUtils.Equation()
                for k in range(shape1[1]):
                    e.addAddend(input2[k][j]*alpha, input1[i][k])

                # Put output variable as the last addend last
                e.addAddend(-1, outputVariables[i][j])
                e.setScalar(-input3[i][j]*beta)
                self.addEquation(e)

    def matMulEquations(self, node, makeEquations):
        """Function to generate equations corresponding to matrix multiplication

        Args:
            node (node): ONNX node representing the MatMul operation
            makeEquations (bool): True if we need to create new variables and write Marabou equations

        :meta private:
        """
        nodeName = node.output[0]

        # Get inputs and determine which inputs are constants and which are variables
        inputName1, inputName2 = node.input
        shape1 = self.shapeMap[inputName1]
        shape2 = self.shapeMap[inputName2]
        if len(shape1) > 2 and shape1[0] == 1:
            shape1 = shape1[1:]
        if len(shape2) > 2 and shape2[0] == 1:
            shape2 = shape2[1:]
        a = np.zeros(shape1)
        b = np.zeros(shape2)
        c = np.matmul(a, b)
        outshape = c.shape
        self.shapeMap[nodeName] = outshape
        if not makeEquations:
            return

        firstInputConstant = False; secondInputConstant = False
        if inputName1 in self.constantMap:
            input1 = self.constantMap[inputName1]
            firstInputConstant = True
        else:
            input1 = self.varMap[inputName1]

        if inputName2 in self.constantMap:
            input2 = self.constantMap[inputName2]
            secondInputConstant = True
        else:
            input2 = self.varMap[inputName2]

        # Broadcast first input to make sure the first input is a matrix
        if len(shape1) == 1:
            shape1 = [1] + shape1
        input1 = input1.reshape(shape1)
        input2 = input2.reshape(shape2)

        # If both inputs are constant, than the output is constant as well, and we don't need new variables or equations
        if firstInputConstant and secondInputConstant:
            self.constantMap[nodeName] = np.matmul(input1,input2)
            return

        # Create new variables
        outputVariables = self.makeNewVariables(nodeName)

        if not firstInputConstant and not secondInputConstant:
            # bi-linear constraints
            self.addBilinearConstraints(shape1, shape2, input1, input2, outputVariables)
        else:
            # Generate equations
            for i in range(shape1[0]):
                # Differentiate between matrix-vector multiplication and matrix-matrix multiplication
                if len(shape2)>1:
                    for j in range(shape2[1]):
                        e = MarabouUtils.Equation()
                        for k in range(shape1[1]):
                            if firstInputConstant:
                                e.addAddend(input1[i][k], input2[k][j])
                            else:
                                e.addAddend(input2[k][j], input1[i][k])

                        # Put output variable as the last addend last
                        e.addAddend(-1, outputVariables[i][j])
                        e.setScalar(0.0)
                        self.addEquation(e)
                else:
                    e = MarabouUtils.Equation()
                    for k in range(shape1[1]):
                        if firstInputConstant:
                            e.addAddend(input1[i][k], input2[k])
                        else:
                            e.addAddend(input2[k], input1[i][k])

                    # Put output variable as the last addend last
                    e.addAddend(-1, outputVariables[i])
                    e.setScalar(0.0)
                    self.addEquation(e)

    def addBilinearConstraints(self, shape1, shape2, input1, input2, outputVariables):
        # Generate equations
        if len(shape2) == 2:
            for i in range(shape1[0]):
                for j in range(shape2[1]):
                    e = MarabouUtils.Equation()
                    for k in range(shape1[1]):
                        v = self.getNewVariable()
                        self.addQuadratic(input1[i][k], input2[k][j], v)
                        e.addAddend(1, v)

                    # Put output variable as the last addend last
                    e.addAddend(-1, outputVariables[i][j])
                    e.setScalar(0.0)
                    self.addEquation(e)
        elif len(shape2) == 3:
            assert(shape1[0] == shape2[0])
            for l in range(shape1[0]):
                for i in range(shape1[1]):
                    for j in range(shape2[2]):
                        e = MarabouUtils.Equation()
                        for k in range(shape1[2]):
                            v = self.getNewVariable()
                            self.addQuadratic(input1[l][i][k], input2[l][k][j], v)
                            e.addAddend(1, v)

                        # Put output variable as the last addend last
                        e.addAddend(-1, outputVariables[l][i][j])
                        e.setScalar(0.0)
                        self.addEquation(e)
        else:
            assert(False)

    def concatEquations(self, node):
        """Function to generate equations corresponding to concat

        Args:
            node (node): ONNX node representing the Concat operation

        :meta private:
        """
        nodeName = node.output[0]

        # Get attributes
        axis = None
        for attr in node.attribute:
            if attr.name == "axis":
                axis = get_attribute_value(attr)

        for inputName in node.input:
            if inputName in self.varMap:
                self.varMap[inputName] = self.varMap[inputName].reshape(self.shapeMap[inputName])

        # Set maps of shape and var
        hasVariable = False
        for inputName in node.input:
            if inputName in self.varMap:
                hasVariable = True
                break
        if hasVariable:
            inputVars = []
            for inputName in node.input:
                if inputName in self.varMap:
                    inputVars.append(self.varMap[inputName])
                else:
                    constantInputs = self.constantMap[inputName].flatten()
                    variableInputs = []
                    for c in constantInputs:
                        x = self.getNewVariable()
                        e = MarabouUtils.Equation()
                        e.addAddend(-1, x)
                        e.setScalar(-c)
                        self.addEquation(e)
                        variableInputs.append(x)
                    variableInputs = np.array(variableInputs).reshape(self.shapeMap[inputName])
                    inputVars.append(variableInputs)

            outputVars = np.concatenate(inputVars, axis)
            self.shapeMap[nodeName] = outputVars.shape
            self.varMap[nodeName] = outputVars
        else:
            inputs = list([self.constantMap[input] for input in node.input])
            outputVars = np.concatenate(inputs, axis)
            self.shapeMap[nodeName] = outputVars.shape
            self.constantMap[nodeName] = outputVars


    def splitEquations(self, node, nodeName, makeEquations):
        """Function to generate equations corresponding to split

        Args:
            node (node): ONNX node representing the Split operation
            nodeName (str): Name of target node
            makeEquations (bool): True if we need to create new variables and write Marabou equations

        :meta private:
        """
        # Get attributes
        axis = None
        split = None
        for attr in node.attribute:
            if attr.name == "axis":
                axis = get_attribute_value(attr)
            if attr.name == "split":
                split = get_attribute_value(attr)
        
        inputName = node.input[0]
        inputVars = torch.from_numpy(self.varMap[inputName]) # rely on torch since split opereation of numpy behaves differently from that of onnx
        inputVars = inputVars.split(split, axis) # tuple

        assert len(inputVars) == len(node.output)

        # Set a shape of target output
        for i in range(len(node.output)):
            if node.output[i] == nodeName:
                self.shapeMap[node.output[i]] = inputVars[i].numpy().shape
                break

        if not makeEquations:
            return

        # Get variables and add quations
        for i in range(len(node.output)):
            if node.output[i] == nodeName:
                reshapedInputVars = inputVars[i].reshape(-1)
                outputVars = self.makeNewVariables(node.output[i]).reshape(-1)
                for j in range(len(reshapedInputVars)):
                    # Add equation
                    e = MarabouUtils.Equation()
                    e.addAddend(1, reshapedInputVars[j])
                    e.addAddend(-1, outputVars[j])
                    e.setScalar(0)
                    self.addEquation(e)
                break

    def resizeEquations(self, node, makeEquations):
        """Function to generate equations corresponding to resize

        Args:
            node (node): ONNX node representing the Resize operation
            makeEquations (bool): True if we need to create new variables and write Marabou equations

        :meta private:
        """
        nodeName = node.output[0]
        inputName = node.input[0]

        # Check number of dimension of input
        inputVars = self.varMap[inputName]
        inputShape = inputVars.shape
        if inputVars.ndim != 4:
            raise NotImplementedError("Marabou only supports resize operator for very specific upsample case used in YOLO now.")

        # Get and check attributes
        coordinate_transformation_mode = None
        cubic_coeff_a = None
        mode = None
        nearest_mode = None

        for attr in node.attribute:
            value = get_attribute_value(attr)
            if attr.name == "coordinate_transformation_mode" and value.decode() == "asymmetric":
                coordinate_transformation_mode = value
            elif attr.name == "cubic_coeff_a" and value == -0.75:
                cubic_coeff_a = value
            elif attr.name == "mode" and value.decode() == "nearest":
                mode = value
            elif attr.name == "nearest_mode" and value.decode() == "floor":
                nearest_mode = value
            else:
                # Marabou supports Resize only very specific case below.
                #  coordinate_transformation_mode: asymmetric
                #  cubic_coeff_a: -0.75
                #  mode: nearest
                #  nearest_mode: floor
                # There are many cases other than the above case according to https://github.com/onnx/onnx/blob/main/docs/Operators.md#resize
                # Please note that we should carefully expand this operation beyond this case.
                raise NotImplementedError("Marabou only supports resize operator for very specific upsample case used in YOLO now.")
        
        # Get scales
        scales = None
        if len(node.input) == 3  and np.all(self.constantMap[node.input[2]] == [1., 1., 2., 2.]):
            scales = [1, 1, 2, 2]
        else:
             raise NotImplementedError("Marabou only supports resize operator for very specific upsample case used in YOLO now.")

        # Set output shape
        outputShape = (inputShape[0], inputShape[1], inputShape[2] * scales[2], inputShape[3] * scales[3])
        self.shapeMap[nodeName] = outputShape

        if not makeEquations:
            return

        # Get variables
        outputVars = self.makeNewVariables(nodeName)

        assert scales[2] * scales[3] * inputVars.size == outputVars.size

        for i in range(outputShape[1]):
            for j in range(outputShape[2]):
                for k in range(outputShape[3]):
                    # Add equation
                    e = MarabouUtils.Equation()
                    e.addAddend(-1, outputVars[0][i][j][k])
                    e.addAddend(1, inputVars[0][i][int(j / 2)][int(k / 2)])
                    e.setScalar(0)
                    self.addEquation(e) 

    def mulEquations(self, node, makeEquations):
        nodeName = node.output[0]

        # Get the inputs
        inputName1, inputName2 = node.input
        shape1 = self.shapeMap[inputName1]
        shape2 = self.shapeMap[inputName2]

        # Get the broadcasted shape
        outShape = shape1
        self.shapeMap[nodeName] = outShape
        if not makeEquations:
            return

        bothInputVariables = False
        if inputName2 in self.constantMap:
            multiple = self.constantMap[inputName2]
            input1 = self.varMap[inputName1]
        elif inputName1 in self.constantMap:
            multiple = self.constantMap[inputName1]
            input1 = self.varMap[inputName2]
        else:
            bothInputVariables = True

        outputVariables = self.makeNewVariables(nodeName)
        outputVariables = outputVariables.reshape(-1)

        if not bothInputVariables:
            input1 = input1.reshape(-1)

            if hasattr(multiple, "__len__") and len(multiple.flatten()) > 1:
                multiple = multiple.reshape(input1.shape)
                for i in range(len(input1)):
                    e = MarabouUtils.Equation()
                    e.addAddend(multiple[i], input1[i])
                    e.addAddend(-1, outputVariables[i])
                    e.setScalar(0.0)
                    self.addEquation(e)
            else:
                multiple = float(multiple)
                for i in range(len(input1)):
                    e = MarabouUtils.Equation()
                    e.addAddend(multiple, input1[i])
                    e.addAddend(-1, outputVariables[i])
                    e.setScalar(0.0)
                    self.addEquation(e)
        else:
            input1 = self.varMap[inputName1].reshape(-1)
            input2 = self.varMap[inputName2].reshape(-1)
            for i in range(len(input1)):
                self.addQuadratic(input1[i], input2[i], outputVariables[i])

        return

    def addEquations(self, node, makeEquations):
        """Function to generate equations corresponding to addition

        Args:
            node (node): ONNX node representing the Add operation
            makeEquations (bool): True if we need to create new variables and write Marabou equations

        :meta private:
        """
        nodeName = node.output[0]

        # Get the inputs
        inputName1, inputName2 = node.input
        shape1 = self.shapeMap[inputName1]
        shape2 = self.shapeMap[inputName2]

        # Get the broadcasted shape
        outShape = getBroadcastShape(shape1, shape2)
        self.shapeMap[nodeName] = outShape
        if not makeEquations:
            return

        # Decide which inputs are variables and which are constants
        firstInputConstant = False; secondInputConstant = False
        if inputName1 in self.constantMap:
            firstInputConstant = True
            input1 = self.constantMap[inputName1]
        else:
            input1 = self.varMap[inputName1]

        if inputName2 in self.constantMap:
            secondInputConstant = True
            input2 = self.constantMap[inputName2]
        else:
            input2 = self.varMap[inputName2]

        # Broadcast inputs to ensure the shapes match
        input1 = np.broadcast_to(input1, outShape)
        input2 = np.broadcast_to(input2, outShape)

        # The shape after broadcasting must match
        assert input1.shape == input2.shape


        # If both inputs to add are constant, then the output is constant too
        # No new variables are needed, we just need to store the output in constantMap
        if firstInputConstant and secondInputConstant:
            self.constantMap[nodeName] = input1 + input2

            return


        # If both inputs are variables, then we need a new variable to represent
        # the sum of the two variables
        elif not firstInputConstant and not secondInputConstant:
            outputVariables = self.makeNewVariables(nodeName)
            input1 = input1.reshape(-1)
            input2 = input2.reshape(-1)
            outputVariables = outputVariables.reshape(-1)
            for i in range(len(input1)):
                e = MarabouUtils.Equation()
                e.addAddend(1, input1[i])
                e.addAddend(1, input2[i])
                e.addAddend(-1, outputVariables[i])
                e.setScalar(0.0)
                self.addEquation(e)
            return

        # Otherwise, we are adding constants to variables.
        # We don't need new equations or new variables if the input variable is the output of a linear equation.
        # Instead, we can just edit the scalar term of the existing linear equation.
        # However, if the input variables are not outputs of linear equations (input variables or outputs of 
        # activation functions) then we will need new equations.
        if firstInputConstant:
            constInput = input1
            varInput = input2
        else:
            constInput = input2
            varInput = input1
        constInput = constInput.reshape(-1)
        varInput = varInput.reshape(-1)

        # Adjust equations to incorporate the constant addition
        numEquationsChanged = 0
        for equ in self.equList:
            (c,var) = equ.addendList[-1]
            assert c == -1
            if var in varInput:
                ind = np.where(var == varInput)[0][0]

                # Adjust the equation
                equ.setScalar(equ.scalar-constInput[ind])
                numEquationsChanged += 1

        # If we changed one equation for every input variable, then
        # we don't need any new equations
        if numEquationsChanged == len(varInput):
            self.varMap[nodeName] = varInput.reshape(outShape)
        else:
            # Otherwise, assert no equations were changed, and we need to create new equations
            assert numEquationsChanged == 0
            outputVariables = self.makeNewVariables(nodeName).reshape(-1)
            for i in range(len(outputVariables)):
                e = MarabouUtils.Equation()
                e.addAddend(1, varInput[i])
                e.addAddend(-1, outputVariables[i])
                e.setScalar(-constInput[i])
                self.addEquation(e)

    def subEquations(self, node, makeEquations):
        """Function to generate equations corresponding to addition

        Args:
            node (node): ONNX node representing the Add operation
            makeEquations (bool): True if we need to create new variables and write Marabou equations

        :meta private:
        """
        nodeName = node.output[0]

        # Get the inputs
        inputName1, inputName2 = node.input
        shape1 = self.shapeMap[inputName1]
        shape2 = self.shapeMap[inputName2]

        # Get the broadcasted shape
        outShape = getBroadcastShape(shape1, shape2)
        self.shapeMap[nodeName] = outShape
        if not makeEquations:
            return

        # Decide which inputs are variables and which are constants
        firstInputConstant = False; secondInputConstant = False
        if inputName1 in self.constantMap:
            firstInputConstant = True
            input1 = self.constantMap[inputName1]
        else:
            input1 = self.varMap[inputName1]

        if inputName2 in self.constantMap:
            secondInputConstant = True
            input2 = self.constantMap[inputName2]
        else:
            input2 = self.varMap[inputName2]

        # Broadcast inputs to ensure the shapes match
        input1 = np.broadcast_to(input1, outShape)
        input2 = np.broadcast_to(input2, outShape)

        # The shape after broadcasting must match
        assert input1.shape == input2.shape

        # If both inputs to add are constant, then the output is constant too
        # No new variables are needed, we just need to store the output in constantMap
        if firstInputConstant and secondInputConstant:
            self.constantMap[nodeName] = input1 - input2
            return

        # If both inputs are variables, then we need a new variable to represent
        # the sum of the two variables
        elif not firstInputConstant and not secondInputConstant:
            outputVariables = self.makeNewVariables(nodeName)
            input1 = input1.reshape(-1)
            input2 = input2.reshape(-1)
            outputVariables = outputVariables.reshape(-1)
            for i in range(len(input1)):
                e = MarabouUtils.Equation()
                e.addAddend(1, input1[i])
                e.addAddend(-1, input2[i])
                e.addAddend(-1, outputVariables[i])
                e.setScalar(0.0)
                self.addEquation(e)
            return

        # Otherwise, we are adding constants to variables.
        # We don't need new equations or new variables if the input variable is the output of a linear equation.
        # Instead, we can just edit the scalar term of the existing linear equation.
        # However, if the input variables are not outputs of linear equations (input variables or outputs of
        # activation functions) then we will need new equations.
        assert(not firstInputConstant)
        constInput = input2
        varInput = input1
        constInput = constInput.reshape(-1)
        varInput = varInput.reshape(-1)

        # Adjust equations to incorporate the constant addition
        numEquationsChanged = 0
        for equ in self.equList:
            (c,var) = equ.addendList[-1]
            assert c == -1
            if var in varInput:
                ind = np.where(var == varInput)[0][0]

                # Adjust the equation
                equ.setScalar(equ.scalar+constInput[ind])
                numEquationsChanged += 1

        # If we changed one equation for every input variable, then
        # we don't need any new equations
        if numEquationsChanged == len(varInput):
            self.varMap[nodeName] = varInput
        else:
            # Otherwise, assert no equations were changed, and we need to create new equations
            assert numEquationsChanged == 0
            outputVariables = self.makeNewVariables(nodeName).reshape(-1)
            for i in range(len(outputVariables)):
                e = MarabouUtils.Equation()
                e.addAddend(1, varInput[i])
                e.addAddend(-1, outputVariables[i])
                e.setScalar(constInput[i])
                self.addEquation(e)

    def divEquations(self, node, makeEquations):
        """Function to generate equations corresponding to addition

        Args:
            node (node): ONNX node representing the Add operation
            makeEquations (bool): True if we need to create new variables and write Marabou equations

        :meta private:
        """
        nodeName = node.output[0]

        # Get the inputs
        inputName1, inputName2 = node.input
        shape1 = self.shapeMap[inputName1]
        shape2 = self.shapeMap[inputName2]

        # Get the broadcasted shape
        outShape = getBroadcastShape(shape1, shape2)
        self.shapeMap[nodeName] = outShape
        if not makeEquations:
            return

        # Decide which inputs are variables and which are constants
        firstInputConstant = False; secondInputConstant = False
        if inputName1 in self.constantMap:
            firstInputConstant = True
            input1 = self.constantMap[inputName1]
        else:
            input1 = self.varMap[inputName1]

        if inputName2 in self.constantMap:
            secondInputConstant = True
            input2 = self.constantMap[inputName2]
        else:
            input2 = self.varMap[inputName2]

        # Broadcast inputs to ensure the shapes match
        input1 = np.broadcast_to(input1, outShape)
        input2 = np.broadcast_to(input2, outShape)

        # The shape after broadcasting must match
        assert input1.shape == input2.shape

        # If both inputs to add are constant, then the output is constant too
        # No new variables are needed, we just need to store the output in constantMap
        if firstInputConstant and secondInputConstant:
            self.constantMap[nodeName] = input1 - input2
            return

        # If both inputs are variables, then we need a new variable to represent
        # the sum of the two variables
        elif not firstInputConstant and not secondInputConstant:
            assert(False)
            return

        # Otherwise, we are adding constants to variables.
        # We don't need new equations or new variables if the input variable is the output of a linear equation.
        # Instead, we can just edit the scalar term of the existing linear equation.
        # However, if the input variables are not outputs of linear equations (input variables or outputs of
        # activation functions) then we will need new equations.
        assert(not firstInputConstant)
        constInput = input2
        varInput = input1
        constInput = constInput.reshape(-1)
        varInput = varInput.reshape(-1)

        # Adjust equations to incorporate the constant addition
        outputVariables = self.makeNewVariables(nodeName).reshape(-1)
        for i in range(len(outputVariables)):
            e = MarabouUtils.Equation()
            e.addAddend(1 / constInput[i], varInput[i])
            e.addAddend(-1, outputVariables[i])
            e.setScalar(0)
            self.addEquation(e)

    def powEquations(self, node, makeEquations):
        """Function to generate equations corresponding to pointwise Relu

        Args:
            node (node): ONNX node representing the Relu operation
            makeEquations (bool): True if we need to create new variables and add new Relus

        :meta private:
        """
        nodeName = node.output[0]
        input1, input2 = node.input[0], node.input[1]
        self.shapeMap[nodeName] = self.shapeMap[input1]
        if not makeEquations:
            return

        assert(input2 in self.constantMap)
        exp = self.constantMap[input2]

        # Get variables
        inputVars = self.varMap[input1].reshape(-1)
        outputVars = self.makeNewVariables(nodeName).reshape(-1)
        assert len(inputVars) == len(outputVars)

        # Generate equations
        for i in range(len(inputVars)):
            currentIn = inputVars[i]
            for j in range(int(exp) - 2):
                currentOut = self.getNewVariable()
                self.addQuadratic(currentIn, inputVars[i], currentOut)
                currentIn = currentOut
            self.addQuadratic(currentIn, inputVars[i], outputVars[i])


    def reluEquations(self, node, makeEquations):
        """Function to generate equations corresponding to pointwise Relu

        Args:
            node (node): ONNX node representing the Relu operation
            makeEquations (bool): True if we need to create new variables and add new Relus

        :meta private:
        """
        nodeName = node.output[0]
        inputName = node.input[0]
        self.shapeMap[nodeName] = self.shapeMap[inputName] 
        if not makeEquations:
            return
        
        # Get variables
        inputVars = self.varMap[inputName].reshape(-1)
        outputVars = self.makeNewVariables(nodeName).reshape(-1)
        assert len(inputVars) == len(outputVars)

        # Generate equations
        for i in range(len(inputVars)):
            self.addRelu(inputVars[i], outputVars[i])
        for f in outputVars:
            self.setLowerBound(f, 0.0)

    def negEquations(self, node, makeEquations):
        """Function to generate equations corresponding to pointwise Relu

        Args:
            node (node): ONNX node representing the Relu operation
            makeEquations (bool): True if we need to create new variables and add new Relus

        :meta private:
        """
        nodeName = node.output[0]
        inputName = node.input[0]
        self.shapeMap[nodeName] = self.shapeMap[inputName]
        if not makeEquations:
            return

        # Get variables
        inputVars = self.varMap[inputName].reshape(-1)
        outputVars = self.makeNewVariables(nodeName).reshape(-1)
        assert len(inputVars) == len(outputVars)

        # Generate equations
        for i in range(len(inputVars)):
            e = MarabouUtils.Equation()
            e.addAddend(-1, inputVars[i])
            e.addAddend(-1, outputVars[i])
            e.setScalar(0)
            self.addEquation(e)



    def cosEquations(self, node, makeEquations):
        """Function to generate equations corresponding to pointwise Relu

        Args:
            node (node): ONNX node representing the Relu operation
            makeEquations (bool): True if we need to create new variables and add new Relus

        :meta private:
        """
        nodeName = node.output[0]
        inputName = node.input[0]
        self.shapeMap[nodeName] = self.shapeMap[inputName]
        if not makeEquations:
            return

        # Get variables
        inputVars = self.varMap[inputName].reshape(-1)
        outputVars = self.makeNewVariables(nodeName).reshape(-1)
        assert len(inputVars) == len(outputVars)

        # Generate equations
        for i in range(len(inputVars)):
            self.addCosine(inputVars[i], outputVars[i])

    def sinEquations(self, node, makeEquations):
        """Function to generate equations corresponding to pointwise Relu

        Args:
            node (node): ONNX node representing the Relu operation
            makeEquations (bool): True if we need to create new variables and add new Relus

        :meta private:
        """
        nodeName = node.output[0]
        inputName = node.input[0]
        self.shapeMap[nodeName] = self.shapeMap[inputName]
        if not makeEquations:
            return

        # Get variables
        inputVars = self.varMap[inputName].reshape(-1)
        outputVars = self.makeNewVariables(nodeName).reshape(-1)
        assert len(inputVars) == len(outputVars)

        # Generate equations
        for i in range(len(inputVars)):
            var = self.getNewVariable()
            e = MarabouUtils.Equation()
            e.addAddend(-1, inputVars[i])
            e.addAddend(-1, var)
            e.setScalar(-3.14159265359 / 2)

            self.addCosine(var, outputVars[i])

    def sigmoidEquations(self, node, makeEquations):
        """Function to generate equations corresponding to Sigmoid

        Args:
            node (node): ONNX node representing the Sigmoid operation
            makeEquations (bool): True if we need to create new variables and add new Sigmoids

        :meta private:
        """
        nodeName = node.output[0]
        inputName = node.input[0]
        self.shapeMap[nodeName] = self.shapeMap[inputName] 
        if not makeEquations:
            return
        
        # Get variables
        inputVars = self.varMap[inputName].reshape(-1)
        outputVars = self.makeNewVariables(nodeName).reshape(-1)
        assert len(inputVars) == len(outputVars)

        # Generate equations
        for i in range(len(inputVars)):
            self.addSigmoid(inputVars[i], outputVars[i])
        for f in outputVars:
            self.setLowerBound(f, 0.0)
            self.setUpperBound(f, 1.0)

    def signEquations(self, node, makeEquations):
        """Function to generate equations corresponding to Sign

        Args:
            node (node): ONNX node representing the Sign operation
            makeEquations (bool): True if we need to create new variables and add new Sign

        :meta private:
        """
        nodeName = node.output[0]
        inputName = node.input[0]
        self.shapeMap[nodeName] = self.shapeMap[inputName]
        if not makeEquations:
            return

        # Get variables
        inputVars = self.varMap[inputName].reshape(-1)
        outputVars = self.makeNewVariables(nodeName).reshape(-1)
        assert len(inputVars) == len(outputVars)

        # Generate equations
        for i in range(len(inputVars)):
            self.addSign(inputVars[i], outputVars[i])
        for f in outputVars:
            self.setLowerBound(f, -1.0)
            self.setUpperBound(f, 1.0)

    def tanhEquations(self, node, makeEquations):
        """Function to generate equations corresponding to Tanh

        Args:
            node (node): ONNX node representing the Tanh operation
            makeEquations (bool): True if we need to create new variables and add new Tanhs

        :meta private:
        """
        nodeName = node.output[0]
        inputName = node.input[0]
        self.shapeMap[nodeName] = self.shapeMap[inputName] 
        if not makeEquations:
            return

        # Get variables
        inputVars = self.varMap[inputName].reshape(-1)
        outputVars = self.makeNewVariables(nodeName).reshape(-1)
        assert len(inputVars) == len(outputVars)
        firstAffine = np.array([self.getNewVariable() for i in range(outputVars.size)])
        sigmoidOutput = np.array([self.getNewVariable() for i in range(outputVars.size)])

        # Generate equations
        for i in range(len(inputVars)):  # tanh(x) = 2 * \sigmoid(2x) - 1
            self.addEquality([inputVars[i], firstAffine[i]], [2.0, -1.0], 0.0, isProperty=False)
            self.addSigmoid(firstAffine[i], sigmoidOutput[i])
            self.addEquality([sigmoidOutput[i], outputVars[i]], [2.0, -1.0], 1.0, isProperty=False)
        for f in outputVars:
            self.setLowerBound(f, -1.0)
            self.setUpperBound(f, 1.0)

    def cleanShapes(self):
        """Remove unused shapes
        
        After constructing equations, remove shapes from self.shapeMap that are part of the graph but not
        relevant for this input query. This is only cosmetic and does not impact Marabou.

        :meta private:
        """
        for nodeName in [name for name in self.shapeMap]:
            if nodeName not in self.varMap and nodeName not in self.constantMap:
                self.shapeMap.pop(nodeName)
                
    def reassignVariable(self, var, numInVars, outVars, newOutVars):
        """Reassign output variable so that output variables follow input variables

        This function computes what the given variable should be when the output variables are 
        moved to come after the input variables.

        Args:
            var (int): Original variable number
            numInVars (int): Number of input variables
            outVars (numpy array of int): Original output variables
            newOutVars (numpy array of int): New output variables

        Returns:
            (int): New variable assignment

        :meta private:
        """
        if var < numInVars:
            return var
        if var in outVars:
            ind = np.where(var == outVars)[0][0]
            return newOutVars[ind]
        return var + len([outVar for outVar in outVars if outVar > var])
    
    def reassignOutputVariables(self):
        """Reassign output variables so output variable numbers follow input variable numbers
        
        Other input parsers assign output variables after input variables and before any intermediate variables.
        This function reassigns the numbers for the output variables and shifts all other variables up to make space.

        :meta private:
        """
        for outputName in self.outputNames:
            if outputName in self.constantMap:
                raise RuntimeError("Output variable %s is a constant, not the output of equations!" % outputName)
        outVars = np.concatenate([self.varMap[outputName].reshape(-1) for outputName in self.outputNames])
        numInVars = np.sum([np.prod(self.shapeMap[inputName]) for inputName in self.inputNames])
        numOutVars = len(outVars)
        newOutVars = np.array(range(numInVars, numInVars+numOutVars))
        
        # Adjust equation variables
        for eq in self.equList:
            for i, (c,var) in enumerate(eq.addendList):
                eq.addendList[i] = (c, self.reassignVariable(var, numInVars, outVars, newOutVars))

        # Adjust equation variables
        for eq in self.additionalEquList:
            for i, (c,var) in enumerate(eq.addendList):
                eq.addendList[i] = (c, self.reassignVariable(var, numInVars, outVars, newOutVars))

        # Adjust relu list
        for i, variables in enumerate(self.reluList):
            self.reluList[i] = tuple([self.reassignVariable(var, numInVars, outVars, newOutVars) for var in variables])

        # Adjust sigmoid list
        for i, variables in enumerate(self.sigmoidList):
            self.sigmoidList[i] = tuple([self.reassignVariable(var, numInVars, outVars, newOutVars) for var in variables])

        # Adjust max pool list
        for i, (elements, outVar) in enumerate(self.maxList):
            newOutVar = self.reassignVariable(outVar, numInVars, outVars, newOutVars)
            newElements = set()
            for var in elements:
                newElements.add(self.reassignVariable(var, numInVars, outVars, newOutVars))
            self.maxList[i] = (newElements, newOutVar)
            
        # Adjust upper/lower bounds
        newLowerBounds = dict()
        newUpperBounds = dict()
        for var in self.lowerBounds:
            newLowerBounds[self.reassignVariable(var, numInVars, outVars, newOutVars)] = self.lowerBounds[var]
        for var in self.upperBounds:
            newUpperBounds[self.reassignVariable(var, numInVars, outVars, newOutVars)] = self.upperBounds[var]
        self.lowerBounds = newLowerBounds
        self.upperBounds = newUpperBounds

        # Assign output variables to the new array
        for outputName in self.outputNames:
            numVars = len(self.varMap[outputName].reshape(-1))
            self.varMap[outputName] = newOutVars[:numVars].reshape(self.shapeMap[outputName])
            newOutVars = newOutVars[numVars:]

        self.outputVars = [self.varMap[outputName] for outputName in self.outputNames]
    
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

def getBroadcastShape(shape1, shape2):
    """Helper function to get the shape that results from broadcasting these shapes together

    Args:
        shape1 (list of int): First shape
        shape2 (list of int): Second shape

    Returns:
        (list of int): Broadcast shape

    :meta private:
    """
    return [l1 if l1 == l2 else max(l1, l2) for l1, l2 in itertools.zip_longest(shape1[::-1], shape2[::-1], fillvalue=1)][::-1]
