from maraboupy import MarabouCore
from maraboupy.Marabou import createOptions

inputQuery = MarabouCore.InputQuery()
inputQuery.setNumberOfVariables(8)

large = 100

equation1 = MarabouCore.Equation()
equation1.addAddend(1, 0)
equation1.addAddend(1, 1)
equation1.addAddend(-1, 2)
equation1.setScalar(0)
inputQuery.addEquation(equation1)

equation2 = MarabouCore.Equation()
equation2.addAddend(1, 0)
equation2.addAddend(-1, 1)
equation2.addAddend(-1, 3)
equation2.setScalar(0)
inputQuery.addEquation(equation2)

equation3 = MarabouCore.Equation()
equation3.addAddend(1, 4)
equation3.addAddend(1, 5)
equation3.addAddend(-1, 6)
equation3.setScalar(0)
inputQuery.addEquation(equation3)

equation4 = MarabouCore.Equation()
equation4.addAddend(1, 4)
equation4.addAddend(-1, 5)
equation4.addAddend(-1, 7)
equation4.setScalar(0)
inputQuery.addEquation(equation4)

# input, output, floor, ceiling
MarabouCore.addClipConstraint(inputQuery, 2, 4, 0, 3)
MarabouCore.addClipConstraint(inputQuery, 3, 5, -0.5, 1)

inputQuery.markInputVariable(0, 0);
inputQuery.markInputVariable(1, 1);

"""
x2 = x0 + x1
x3 = x0 - x1

x4 = clip(x2, 0, 3)
x5 = clip(x3, -0.5, 1)
x6 = x4 + x5
x7 = x4 - x5
"""

inputQuery.setLowerBound(0, -1.1)
inputQuery.setUpperBound(0, -1)
inputQuery.setLowerBound(1, -2.1)
inputQuery.setUpperBound(1, -2)

options = createOptions()
exitCode, bounds, stats = MarabouCore.calculateBounds(inputQuery, options, "")
print(bounds)
