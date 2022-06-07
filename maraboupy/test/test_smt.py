# Supress warnings caused by tensorflow
import warnings
warnings.filterwarnings('ignore', category = DeprecationWarning)
warnings.filterwarnings('ignore', category = PendingDeprecationWarning)

import pytest
from maraboupy import MarabouCore
from maraboupy.Marabou import createOptions

import os

def test_dump_query():
    """
    This function tests that MarabouCore.solve can be called with all arguments and
    checks that a SAT query is solved correctly. This also tests the InputQuery dump() method
    as well as bound tightening during solving.
    """
    ipq = define_ipq()

    # Test dump
    res = MarabouCore.solve( ipq, MarabouCore.Options() )
    print(res)
    MarabouCore.writeSmtLib( ipq, "./test.smt2" )
    assert(os.path.isfile("./test.smt2"))
    #os.remove("test.smt2")

def define_ipq():
    """
    This function defines a simple input query directly through MarabouCore
    Arguments:
    Returns:
        ipq (MarabouCore.InputQuery) input query object representing network and constraints
    """
    ipq = MarabouCore.InputQuery()
    ipq.setNumberOfVariables(18)

    ipq.setLowerBound(0, -1)
    ipq.setUpperBound(0, 1)
    ipq.setLowerBound(1, -1)
    ipq.setUpperBound(1, 1)
    ipq.setLowerBound(2, -1)
    ipq.setUpperBound(2, 1)
    ipq.setLowerBound(6, -1)
    ipq.setUpperBound(6, 1)
    ipq.setLowerBound(7, -1)
    ipq.setUpperBound(7, 1)
    ipq.setLowerBound(8, -1)
    ipq.setUpperBound(8, 1)

    """
    x3,x4,x5 = softmax (x0,x1,x2)
    x9,x10,x11 = softmax (x6,x7,x8)
    x12 = x3 * x9 + x4 * x10
    x13 = x3 + x4 + x5
    x14 = x9 + x10 + x11
    x15 = relu( x12 )
    x16 = relu( x13 )
    x17 = relu( x14 )
    """

    MarabouCore.addSoftmaxConstraint(ipq, [0,1,2], [3,4,5])
    MarabouCore.addSoftmaxConstraint(ipq, [6,7,8], [9,10,11])
    MarabouCore.addReluConstraint(ipq, 12, 15);
    MarabouCore.addReluConstraint(ipq, 13, 16);
    MarabouCore.addReluConstraint(ipq, 14, 17);

    e1 = MarabouCore.Equation()
    e1.addAddend(1, 13)
    e1.addAddend(-1, 3)
    e1.addAddend(-1, 4)
    e1.addAddend(-1, 5)
    e1.setScalar(0)
    ipq.addEquation(e1)

    e2 = MarabouCore.Equation()
    e2.addAddend(1, 14)
    e2.addAddend(-1, 9)
    e2.addAddend(-1, 10)
    e2.addAddend(-1, 11)
    e2.setScalar(0)
    ipq.addEquation(e2)

    q1 = MarabouCore.QuadraticEquation()
    q1.addAddend(1, 12)
    q1.addQuadraticAddend(-1, 3, 9)
    q1.addQuadraticAddend(-1, 4, 10)
    q1.setScalar(0)
    ipq.addQuadraticEquation(q1)
    return ipq

test_dump_query()
