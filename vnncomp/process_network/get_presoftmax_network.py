import onnx
import onnxruntime
import sys

# Usage: get_presoftmax_network.py [network] [new network]

# Load the onnx file
model = onnx.load(sys.argv[1])

# Get the output node name
output_node = model.graph.output[0].name

# Check if the output node is a softmax node
softmax_node = None
for node in model.graph.node:
    if node.op_type == "Softmax" and node.output[0] == output_node:
        softmax_node = node
        break

# If softmax is applied, create a new onnx file with pre-softmax logit as output
if softmax_node is not None:
    # Get the input node name of the softmax node
    input_node = softmax_node.input[0]

    # Remove the softmax node from the graph
    model.graph.node.remove(softmax_node)

    # Change the output node name to the input node name
    model.graph.output[0].name = input_node

    print("New onnx file created with pre-softmax logit as output.")
else:
    print("No softmax applied to the output logit.")

onnx.save(model, sys.argv[2])
