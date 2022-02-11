'''
ONNX Example
====================

Top contributors (to current version):
  - Kyle Julian
  
This file is part of the Marabou project.
Copyright (c) 2017-2019 by the authors listed in the file AUTHORS
in the top-level source directory) and their institutional affiliations.
All rights reserved. See the file COPYING in the top-level source
directory for licensing information.
'''

import sys
sys.path.append("/home/haozewu/Projects/addSoISupport/Marabou")
from maraboupy import Marabou
import numpy as np

# %%
# Set the Marabou option to restrict printing
options = Marabou.createOptions(verbosity = 0)


# %%
# Convolutional network with max-pool example
# -------------------------------------------
print("\nConvolutional Network with Max Pool Example")
filename = '../../../modelgts.onnx'
network = Marabou.read_onnx(filename)

# %%
# Get the input and output variable numbers; [0] since first dimension is batch size
inputVars = network.inputVars[0]
outputVars = network.outputVars

# %% 
# Test Marabou equations against onnxruntime at an example input point
inputPoint = np.ones(inputVars.shape)
marabouEval = network.evaluateWithMarabou([inputPoint], options = options)
onnxEval = network.evaluateWithoutMarabou([inputPoint])

# %%
# The two evaluations should produce the same result
print("Marabou Evaluation:")
print(marabouEval)
print("\nONNX Evaluation:")
print(onnxEval)
print("\nDifference:")
print(onnxEval - marabouEval)
assert max(abs(onnxEval - marabouEval)) < 1e-6
