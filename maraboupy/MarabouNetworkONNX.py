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

MarabouNetworkONNX represents neural networks with piecewise linear constraints derived from the ONNX format
'''
import numpy as np
import onnx
import onnxruntime
from onnx import numpy_helper
from onnx.helper import get_attribute_value
from maraboupy import MarabouUtils
from maraboupy import MarabouNetwork
from maraboupy import MarabouCore
from onnx import TensorProto
import itertools
import torch
import os
import re
from copy import copy

class MarabouNetworkONNX(MarabouNetwork.MarabouNetwork):
    """Constructs a MarabouNetworkONNX object from an ONNX file

    Args:
        filename (str): Path to the ONNX file
        inputNames: (list of str, optional): List of node names corresponding to inputs
        outputNames: (list of str, optional): List of node names corresponding to outputs
        vnnlibFilename (str): Optional argument of filename to vnnlib file containing a property

    Returns:
        :class:`~maraboupy.Marabou.marabouNetworkONNX.marabouNetworkONNX`
    """
    def __init__(self, filename, inputNames=None, outputNames=None, reindexOutputVars=True, vnnlibFilename=None):
        super().__init__()
        self.readONNX(filename, inputNames, outputNames, reindexOutputVars=reindexOutputVars, vnnlibFilename=vnnlibFilename)

    def clear(self):
        """Reset values to represent empty network
        """
        super().clear()
        self.madeGraphEquations = []
        self.varMap = dict()
        self.constantMap = dict()
        self.shapeMap = dict()
        self.vnnlibMap = dict()
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

    def readONNX(self, filename, inputNames, outputNames, reindexOutputVars=True, vnnlibFilename=None):
        """Read an ONNX file and create a MarabouNetworkONNX object

        Args:
            filename: (str): Path to the ONNX file
            inputNames: (list of str): List of node names corresponding to inputs
            outputNames: (list of str): List of node names corresponding to outputs
            reindexOutputVars: (bool): Reindex the variables so that the output variables are immediate after input variables.
            vnnlibFilename (str): Optional argument of filename to vnnlib file containing a property

        :meta private:
        """
        self.filename = filename
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
                raise RuntimeError("Input %s not found in graph!" % name)
        for name in outputNames:
            if not len([nde for nde in self.graph.node if name in nde.output]):
                raise RuntimeError("Output %s not found in graph!" % name)

        self.inputNames = inputNames
        self.outputNames = outputNames

        # Process the shapes and values of the graph while making Marabou equations and constraints
        self.foundnInputFlags = 0
        self.processGraph()

        # If the given inputNames/outputNames specify only a portion of the network, then we will have
        # shape information saved not relevant to the portion of the network. Remove extra shapes.
        self.cleanShapes()

        if reindexOutputVars:
            # Other Marabou input parsers assign output variables immediately after input variables and before any
            # intermediate variables. This function reassigns variable numbering to match other parsers.
            # If this is skipped, the output variables will be the last variables defined.
            self.reassignOutputVariables()
        else:
            self.outputVars = [self.varMap[outputName] for outputName in self.outputNames]

        if vnnlibFilename:
            self.loadPropertyWithVnnlib(vnnlibFilename)

    def splitNetworkAtNode(self, nodeName, networkNamePreSplit=None,
                           networkNamePostSplit=None):
        """
        Cut the current onnx file at the given node to create two networks.
        The output of the first network is the output of the given node.
        The second network expects its input to be the output of the first network.

        Return True if the split is successful.

        Args:
            nodeName (str): Name of node at which we want to cut the network
            networkNamePreSplit(str): If given, store the pre-split network at the given path.
            networkNamePostSplit(str): If given, store the post-split network at the given path.

        :meta private:
        """
        outputName = self.getNode(nodeName).output[0]
        if networkNamePreSplit is not None:
            try:
                onnx.utils.extract_model(self.filename, networkNamePreSplit,
                                         input_names=self.inputNames,
                                         output_names=[outputName])
            except Exception as error:
                print("Error when trying to create pre-split network: ", error)
                if os.path.isfile(networkNamePreSplit):
                    os.remove(networkNamePreSplit)
                return False
        if networkNamePostSplit is not None:
            try:
                onnx.utils.extract_model(self.filename, networkNamePostSplit,
                                         input_names=[outputName],
                                         output_names=self.outputNames)
            except Exception as error:
                print("Error when trying to create post-split network: ", error)
                if os.path.isfile(networkNamePostSplit):
                    os.remove(networkNamePostSplit)
                return False

        self.outputNames = [outputName]
        return True

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

        # Compute node's shape and create Marabou equations as needed
        self.makeMarabouEquations(nodeName, makeEquations)

        # Create new variables when we find one of the inputs
        if nodeName in self.inputNames:
            self.makeNewVariables(nodeName)
            self.inputVars += [np.array(self.varMap[nodeName])]

    def makeMarabouEquations(self, nodeName, makeEquations):
        """Compute the shape and values of a node assuming the input shapes and values have been computed already.

        Args:
            nodeName (str): Name of node for which we want to compute the output shape
            makeEquations (bool): Create Marabou equations for this node if True

        :meta private:
        """
        node = self.getNode(nodeName)
        if node.op_type == 'Constant':
            self.constant(node)
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
        elif node.op_type == 'Unsqueeze':
            self.unsqueeze(node)
        elif node.op_type == 'Squeeze':
            self.squeeze(node)
        elif node.op_type == 'Einsum':
            self.einsum(node, makeEquations)
        elif node.op_type == "BatchNormalization":
            self.batchNorm(node, makeEquations)
        elif node.op_type == 'Concat':
            self.concatEquations(node)
        elif node.op_type == "MaxPool":
            self.maxpoolEquations(node, makeEquations)
        elif node.op_type == "Conv":
            self.convEquations(node, makeEquations)
        elif node.op_type == 'Gemm':
            self.gemmEquations(node, makeEquations)
        elif node.op_type == 'MatMul':
            self.matMulEquations(node, makeEquations)
        elif node.op_type == 'Mul':
            self.mulEquations(node, makeEquations)
        elif node.op_type == 'Div':
            self.divEquations(node, makeEquations)
        elif node.op_type == 'Add':
            self.addEquations(node, makeEquations)
        elif node.op_type == 'Relu':
            self.reluEquations(node, makeEquations)
        elif node.op_type == 'Clip':
            self.reluEquations(node, makeEquations)
        elif node.op_type == 'Sigmoid':
            self.sigmoidEquations(node, makeEquations)
        elif node.op_type == 'Split':
            self.splitEquations(node, nodeName, makeEquations)
        elif node.op_type == 'Resize':
            self.resizeEquations(node, makeEquations)
        elif node.op_type == 'Tanh':
            self.tanhEquations(node, makeEquations)
        elif node.op_type == 'Softmax':
            self.softmaxEquations(node, makeEquations)
        elif node.op_type == 'Sub':
            self.subEquations(node, makeEquations)
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
        if reshapeVals[0] == 0:
            reshapeVals[0] = 1

        self.shapeMap[nodeName] = list(np.zeros(self.shapeMap[inputName1]).reshape(reshapeVals).shape)
        if inputName1 in self.varMap:
            self.varMap[nodeName] = copy(self.varMap[inputName1]).reshape(self.shapeMap[nodeName])
        elif inputName1 in self.constantMap:
            self.constantMap[nodeName] = self.constantMap[inputName1].reshape(self.shapeMap[nodeName])
      

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
        self.shapeMap[nodeName] = [self.shapeMap[inputName][p] for p in perm]
        if inputName in self.varMap:
            self.varMap[nodeName] = \
            np.transpose(self.varMap[node.input[0]].reshape(self.shapeMap[node.input[0]]),
                         perm)
        elif inputName in self.constantMap:
            self.constantMap[nodeName] = np.transpose(self.constantMap[inputName], perm)

    def unsqueeze(self, node):
        """Function representing unsqueeze

        Args:
            node (node): ONNX node representing unsqueeze operation

        :meta private:
        """
        nodeName = node.output[0]
        inputName, axisName = node.input

        axis = self.constantMap[axisName]
        if inputName in self.varMap:
            vars = copy(self.varMap[inputName])
            for a in axis:
                vars = np.expand_dims(vars, axis=a)
            self.varMap[nodeName] = vars
            self.shapeMap[nodeName] = vars.shape
        elif inputName in self.constantMap:
            c = copy(self.constantMap[inputName])
            for a in axis:
                c = np.expand_dims(c, axis=a)
            self.constantMap[nodeName] = c
            self.shapeMap[nodeName] = c.shape
        return

    def squeeze(self, node):
        """Function representing squeeze

        Args:
            node (node): ONNX node representing squeeze operation

        :meta private:
        """
        nodeName = node.output[0]
        inputName, axisName = node.input

        axis = self.constantMap[axisName]

        axis_ = copy(axis)
        if inputName in self.varMap:
            vars = copy(self.varMap[inputName])
            for i in range(len(axis)):
                vars = np.squeeze(vars, axis_[i])
                for j in range(len(axis))[i+1:]:
                    axis_[j] -= 1
            self.varMap[nodeName] = vars
            self.shapeMap[nodeName] = vars.shape
        return

    def einsum(self, node, makeEquations):
        """Function to generate equations for a BatchNormalization

        Args:
            node (node): ONNX node representing the BatchNormalization operation

        :meta private
        """

        nodeName = node.output[0]
        inputName1, inputName2 = node.input

        s1 = self.shapeMap[inputName1]
        s2 = self.shapeMap[inputName2]

        assert(s1 == [1,2,2,4] and s2 == [2,4,8])
        self.shapeMap[nodeName] = [s1[0], s1[1], s2[2]]

        inputVars = self.varMap[inputName1]
        scalars = self.constantMap[inputName2]

        outputVars = self.makeNewVariables(nodeName)

        # Get attributes
        exp = None
        for attr in node.attribute:
            if attr.name == "equation":
                exp = get_attribute_value(attr)
        assert(exp == b'abcd,cde->abe')

        for i in range(2):
            for j in range(8):
                e = MarabouUtils.Equation()
                for k in range(2):
                    for l in range(4):
                        e.addAddend(scalars[k][l][j], inputVars[0][i][k][l])
                e.addAddend(-1, outputVars[0][i][j])
                e.setScalar(0)
                self.addEquation(e)
        return

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
                e.addAddend(-1, outputVars[i][j])
                e.addAddend(1 / np.sqrt(input_variances[i] + epsilon) * scales[i], inputVars[i][j])
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
        if len(node.input) == 3:
            inputName1, inputName2, inputName3 = node.input
        else:
            inputName1, inputName2 = node.input
            inputName3 = None
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
        if inputName3:
            input3 = self.constantMap[inputName3]

        # Transpose inputs
        if transA:
            input1 = np.transpose(input1)
        if transB:
            input2 = np.transpose(input2)
        if inputName3:
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
                if inputName3:
                    e.setScalar(-input3[i][j]*beta)
                else:
                    e.setScalar(0)
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

        # Set maps of shape and var
        inputVars = list([self.varMap[input] for input in node.input])
        outputVars = np.concatenate(inputVars, axis)
        self.shapeMap[nodeName] = outputVars.shape
        self.varMap[nodeName] = outputVars

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
                    e.addAddend(-1, outputVars[j])
                    e.addAddend(1, reshapedInputVars[j])
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

        # Set maps of shape and var
        inputVars = list([self.varMap[input] for input in node.input])
        outputVars = np.concatenate(inputVars, axis)
        self.shapeMap[nodeName] = outputVars.shape
        self.varMap[nodeName] = outputVars

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
                    e.addAddend(-1, outputVars[j])
                    e.addAddend(1, reshapedInputVars[j])
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
        # shape2 = self.shapeMap[inputName2] # comment out since this is never used.


        # Get the broadcasted shape
        outShape = shape1
        self.shapeMap[nodeName] = outShape
        if not makeEquations:
            return

        multiple = self.constantMap[inputName2]
        input1 = self.varMap[inputName1]
        outputVariables = self.makeNewVariables(nodeName)
        input1 = input1.reshape(-1)
        outputVariables = outputVariables.reshape(-1)

        for i in range(len(input1)):
            e = MarabouUtils.Equation()
            e.addAddend(multiple, input1[i])
            e.addAddend(-1, outputVariables[i])
            e.setScalar(0.0)
            self.addEquation(e)
        return

    def divEquations(self, node, makeEquations):
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

        multiple = self.constantMap[inputName2]
        input1 = self.varMap[inputName1]
        outputVariables = self.makeNewVariables(nodeName)
        input1 = input1.reshape(-1)
        outputVariables = outputVariables.reshape(-1)

        for i in range(len(input1)):
            e = MarabouUtils.Equation()
            e.addAddend(1/multiple, input1[i])
            e.addAddend(-1, outputVariables[i])
            e.setScalar(0.0)
            self.addEquation(e)
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
            
            self.varMap[outputName] = outputVariables.reshape(outShape)
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
            self.varMap[nodeName] = copy(varInput).reshape(outShape)
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
            self.varMap[nodeName] = copy(outputVariables).reshape(outShape)

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

    def subEquations(self, node, makeEquations):
        """Function to generate equations corresponding to subtraction

        Args:
            node (node): ONNX node representing the Sub operation
            makeEquations (bool): True if we need to create new variables and add new Relus

        :meta private:
        """
        nodeName = node.output[0]
        inputName1, inputName2 = node.input[0], node.input[1]
        assert inputName1 in self.shapeMap and inputName2 in self.shapeMap
        assert self.shapeMap[inputName1] == self.shapeMap[inputName2]
        self.shapeMap[nodeName] = self.shapeMap[inputName1]

        if not makeEquations:
            return

        assert inputName1 in self.varMap and inputName2 in self.constantMap

        # Get variables
        inputVars = self.varMap[inputName1].reshape(-1)
        outputVars = self.makeNewVariables(nodeName).reshape(-1)
        constants = self.constantMap[inputName2].reshape(-1)
        assert len(inputVars) == len(outputVars) == len(constants)

        # Generate equations
        for i in range(len(inputVars)):
            e = MarabouUtils.Equation()
            e.addAddend(1, inputVars[i])
            e.addAddend(-1, outputVars[i])
            e.setScalar(-constants[i])
            self.addEquation(e)

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

        # Adjust bilinear list
        for i, variables in enumerate(self.bilinearList):
            self.bilinearList[i] = tuple([self.reassignVariable(var, numInVars, outVars, newOutVars) for var in variables])

        # Adjust softmax list
        for i, (inputs, outputs) in enumerate(self.softmaxList):
            newInputs = []
            for var in inputs:
                newInputs.append(self.reassignVariable(var, numInVars, outVars, newOutVars))
            newOutputs = []
            for var in outputs:
                newOutputs.append(self.reassignVariable(var, numInVars, outVars, newOutVars))

            self.softmaxList[i] = (newInputs, newOutputs)

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

    def loadPropertyWithVnnlib(self, vnnlibFilename):
        """Loads the property from the given vnnlib file

        Args:
            vnnlibFilename (str): Filename for the vnnlib file

        Returns:
            None
        """
        with open(vnnlibFilename, 'r') as f:
            lines = f.readlines()

        vnnlib_content = ""
        for i in range(len(lines)):
            if lines[i] == "" or lines[i].startswith(";"):
                continue

            vnnlib_content += lines[i].strip()

        vnnlib_content = "(" + vnnlib_content + ")"
        vnnlib_commands = make_tree(vnnlib_content)

        for command in vnnlib_commands:
            self.parse_command(command)

    def parse_command(self, command):
        """
        Parses a single VNN-LIB command
        Args:
            command (lst): The command to pars

        Returns:
            None
        """
        assert isinstance(command, list) and len(command) >= 2
        command_type = command[0]
        assert isinstance(command_type, str)

        if command_type == "declare-const":
            self.parse_declare_const(command[1], command[2])
        elif command_type == "assert":
            self.parse_assert(command[1])
        else:
            raise NotImplementedError(f"VNN-LIB statement type '{command_type} not implemented")

    def parse_declare_const(self, var_name, var_type):
        """
        Parses a single "declare-const" command
        Args:
            var_name (str): Declared variable name
            var_type (str): Declared variable type

        Returns:
            None
        """
        assert isinstance(var_name, str) and isinstance(var_type, str)

        if var_type != "Real":
            raise NotImplementedError("Declaration of variables not of type 'Real' is not supported")

        var_kind, var_index = var_name.split("_")
        if not var_index.isnumeric():
            raise RuntimeError("Variable index should be a non-negative integer number")

        if var_kind == 'X':
            if int(var_index) >= len(self.inputVars[0].reshape(-1)):
                raise RuntimeError("There is an input variable declaration in the VNN-LIB specification that does not "
                                   "exists in the ONNX network itself")
            self.vnnlibMap[var_name] = (int(var_index), var_type)
        elif var_kind == 'Y':
            if int(var_index) >= len(self.outputVars[0][0].reshape(-1)):
                raise RuntimeError("There is an output variable declaration in the VNN-LIB specification that does not "
                                   "exists in the ONNX network itself")
            self.vnnlibMap[var_name] = (int(var_index) + len(self.inputVars[0].reshape(-1)), var_type)
        else:
            raise RuntimeError("All variable name should should begin with 'X_' for input variables, "
                               "and 'Y_' for output variables in variable declarations")


    def parse_assert(self, assert_command):
        """
        Parses a single "assert" command
        Args:
            assert_command (list): The 'assert' command to parse

        Returns:
            None
        """
        assert isinstance(assert_command, list) and len(assert_command) >= 2
        operator = assert_command[0]
        if operator in {"<=", ">=", "and"}:
            equations = self.parse_condition(assert_command)
            for eq in equations:
                self.addEquation(eq)
        elif operator == "or": # TODO: change after fixing MarabouUtils.Equation for Disjunction constraint
            disjuncts = self.parse_condition(assert_command)
            new_disjuncts = []
            for disjunct in disjuncts:
                new_disjunct = []
                for eq in disjunct:
                    new_eq = MarabouCore.Equation(eq.EquationType)
                    for (c, v) in eq.addendList:
                        new_eq.addAddend(c, v)
                    new_eq.setScalar(eq.scalar)
                    new_disjunct.append(new_eq)

                new_disjuncts.append(new_disjunct)

            self.addDisjunctionConstraint(new_disjuncts)
        else:
            raise NotImplementedError(f"assert operator {operator} not implemented")

    def parse_condition(self, cond_command):
        """
        Parses a single condition
        Args:
            cond_command: Condition to parse

        Returns:
            list of Equations, or list of list of Equations (in case of disjunction)
        """
        assert isinstance(cond_command, list)
        if len(cond_command) < 2:
            raise RuntimeError("Each condition command should contain at least 1 argument")

        operator = cond_command[0]
        if operator == "<=":
            if len(cond_command) != 3:
                raise RuntimeError("Assert command of type '<=' only support 2 terms as arguments")

            arg1, arg2 = self.parse_term(cond_command[1]), self.parse_term(cond_command[2])
            return [self.parse_le(arg1, arg2)]
        elif operator == ">=":
            if len(cond_command) != 3:
                raise RuntimeError("Assert command of type '>=' only support 2 terms as arguments")

            arg1, arg2 = self.parse_term(cond_command[1]), self.parse_term(cond_command[2])
            return [self.parse_le(arg2, arg1)]
        elif operator == "and":
            cond_equations = []
            for sub_cond_command in cond_command[1:]:
                if not isinstance(sub_cond_command, list):
                    raise RuntimeError("Sub conditions of 'and' should be between parentheses characters '(' and ')'")

                cond_equations += self.parse_condition(sub_cond_command)

            return cond_equations
        elif operator == "or":
            disjuncts = []
            for sub_cond_command in cond_command[1:]:
                if not isinstance(sub_cond_command, list):
                    raise RuntimeError("Sub conditions of 'or' should be between parentheses characters '(' and ')'")

                disjuncts.append(self.parse_condition(sub_cond_command))

            return disjuncts

    def parse_term(self, term):
        """
        Parses a single term
        Args:
            term (list): The term to parse

        Returns:
            a tuple of parsing results, depending on term's operator type
        """
        if isinstance(term, list):
            assert len(term) >= 2
            operator = term[0]
            if operator == "+":
                return self.parse_add(term)
            elif operator == "-":
                return self.parse_sub(term)
            elif operator == "*":
                return self.parse_mul(term)
        elif term in self.vnnlibMap:
            return self.vnnlibMap[term][0], "var"
        else:
            try:
                const_value = float(term)
                return const_value, "const"
            except ValueError:
                raise RuntimeError(f"Term {term} is not a variable declaration, a number or a function")

    def parse_le(self, arg1, arg2):
        """
        Parses an '<=' condition
        Args:
            arg1 (tuple): Left term of '<='
            arg2 (tuple): Right term of '<='

        Returns:
            Equation representing this condition
        """
        assert isinstance(arg1, tuple) and isinstance(arg2, tuple)
        assert isinstance(arg1[-1], str) and isinstance(arg2[-1], str)

        eq = MarabouUtils.Equation(MarabouCore.Equation.LE)
        scalar = 0

        if arg1[-1] == "const":
            assert len(arg1) == 2 and isinstance(arg1[0], float)
            scalar -= arg1[0]
        elif arg1[-1] == "var":
            assert len(arg1) == 2 and isinstance(arg1[0], int)
            eq.addAddend(1, arg1[0])
        elif arg1[-1] == "+":
            assert len(arg1) == 3 and isinstance(arg1[0], list) and isinstance(arg1[1], float)
            for v in arg1[0]:
                eq.addAddend(1, v)
            scalar -= arg1[1]
        elif arg1[-1] == "-":
            assert len(arg1) == 3 and isinstance(arg1[0], tuple) and isinstance(arg1[1], tuple)
            assert len(arg1[0]) == 2 and len(arg1[1]) == 2
            if arg1[0][1] == "const":
                scalar -= arg1[0][0]
            else: # arg1[0][1] == "var"
                eq.addAddend(1, arg1[0][0])
            if arg1[1][1] == "const":
                scalar -= arg1[1][0]
            else: # arg1[1][1] == "var"
                eq.addAddend(-1, arg1[1][0])
        elif arg1[-1] == "*":
            assert len(arg1) == 3 and (isinstance(arg1[0], int) or arg1[0] is None) and isinstance(arg1[1], float)
            if arg1[0] is None:
                scalar -= arg1[1]
            else:
                eq.addAddend(arg1[1], arg1[0])

        if arg2[-1] == "const":
            assert len(arg2) == 2 and isinstance(arg2[0], float)
            scalar += arg2[0]
        elif arg2[-1] == "var":
            assert len(arg2) == 2 and isinstance(arg2[0], int)
            eq.addAddend(-1, arg2[0])
        elif arg2[-1] == "+":
            assert len(arg2) == 3 and isinstance(arg2[0], list) and isinstance(arg2[1], float)
            for v in arg2[0]:
                eq.addAddend(-1, v)
            scalar += arg2[1]
        elif arg2[-1] == "-":
            assert len(arg2) == 3 and isinstance(arg2[0], tuple) and isinstance(arg2[1], tuple)
            assert len(arg2[0]) == 2 and len(arg2[1]) == 2
            if arg2[0][1] == "const":
                scalar += arg2[0][0]
            else:  # arg2[0][1] == "var"
                eq.addAddend(-1, arg2[0][0])
            if arg2[1][1] == "const":
                scalar += arg2[1][0]
            else:  # arg2[1][1] == "var"
                eq.addAddend(1, arg2[1][0])
        elif arg2[-1] == "*":
            assert len(arg2) == 3 and (isinstance(arg2[0], int) or arg2[0] is None) and isinstance(arg2[1], float)
            if arg2[0] is None:
                scalar += arg2[1]
            else:
                eq.addAddend(-arg2[1], arg2[0])

        eq.setScalar(scalar)
        return eq

    def parse_add(self, add_term):
        """
        Parses a single '+' term
        Args:
            add_term (list): The '+' term to parse

        Returns:
            A 3-tuple of (list of variables, sum of constants, '+')
        """
        assert isinstance(add_term, list)
        assert add_term[0] == "+"

        if len(add_term) < 3:
            raise RuntimeError("A '+' term should contain at lease 2 arguments")

        args = [self.parse_term(add_term[i]) for i in range(1, len(add_term))]
        variables = []
        const_total = 0.
        for arg in args:
            assert isinstance(arg, tuple)
            if len(arg) != 2 or not (arg[1] == "const" or arg[1] == "var"):
                raise RuntimeError("All arguments of a '+' term should be declared variable names, or constant numbers")

            if arg[1] == "const":
                const_total += arg[0]
            elif arg[1] == "var":
                variables.append(arg[0])

        return variables, const_total, "+"

    def parse_sub(self, sub_term):
        """
        Parses a single '-' term
        Args:
            sub_term (list): The '-' term to parse

        Returns:
            A 3-tuple of (term of left argument, term of right argument, '-') in case of 2 arguments,
            or a 2-tuple of (-argument, "const") in case of 1 constant number argument
        """
        assert isinstance(sub_term, list)
        assert sub_term[0] == "-"

        if len(sub_term) not in {2, 3}:
            raise RuntimeError("A '-' term should contain 1 or 2 arguments")

        arg1 = self.parse_term(sub_term[1])
        assert isinstance(arg1, tuple)
        if len(arg1) != 2 or not (arg1[1] == "const" or arg1[1] == "var"):
            raise RuntimeError("The arguments of a '-' term should be declared variable names, or constant numbers")

        if len(sub_term) == 3:
            # subtraction between two terms
            arg2 = self.parse_term(sub_term[2])
            assert isinstance(arg2, tuple)
            if len(arg2) != 2 or not (arg2[1] == "const" or arg2[1] == "var"):
                raise RuntimeError("The arguments of a '-' term should be declared variable names, or constant numbers")

            return arg1, arg2, "-"
        else:
            # negation to constant number
            if arg1[1] == "const":
                return -arg1[0], "const"
            else:
                raise NotImplementedError("Using the '-' term for one argument other than constant number is not supported")

    def parse_mul(self, mul_term):
        """
        Parses a single '*' term
        Args:
            mul_term (tuple): The '*' term to parse

        Returns:
            A 3-tuple of (variable or None, multiplication of constant numbers, '*')
        """
        assert isinstance(mul_term, list)
        assert mul_term[0] == "*"

        if len(mul_term) < 3:
            raise RuntimeError("A '*' term should contain at lease 2 arguments")

        args = [self.parse_term(mul_term[i]) for i in range(1, len(mul_term))]
        variable = None
        const_total = 1.

        for arg in args:
            assert isinstance(arg, tuple)
            if len(arg) != 2 or not (arg[1] == "const" or arg[1] == "var"):
                raise RuntimeError("All arguments of a '*' term should be declared variable names, or constant numbers")

            if arg[1] == "const":
                const_total *= arg[0]
            elif arg[1] == "var":
                if variable is None:
                    variable = arg[0]
                else:
                    raise RuntimeError("In a '*' term only 1 variable argument is allowed")

        return variable, const_total, "*"

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


def make_tree(content):
    """Helper function to get the statements of given vnnlib content file split into lists

    Args:
        content (str): Content of vnnlib file (filtered after removing comments)

    Returns:
        (nested lists of str): list of statements in vnnlib content, possibly with more nested lists

    :meta private:
    """
    items = re.findall(r"\(|\)|[\w\-\\.]+|<=|>=|\+|-|\*", content)

    def req(index):
        result = []
        item = items[index]
        while item != ")":
            if item == "(":
                subtree, index = req(index + 1)
                result.append(subtree)
            else:
                result.append(item)
            index += 1
            item = items[index]
        return result, index

    return req(1)[0]

