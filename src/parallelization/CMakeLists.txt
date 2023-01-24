#file(GLOB SRCS "*.cpp")

file(GLOB HEADERS
  SnCDivideStrategy.h
  )


target_sources(${MARABOU_LIB} PRIVATE ${SRCS})
target_include_directories(${MARABOU_LIB} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_sources(${MARABOU_TEST_LIB} PRIVATE ${SRCS})
target_include_directories(${MARABOU_TEST_LIB} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

set (PARALLELIZATION_TESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")
macro(parallelization_add_unit_test name)
    set(USE_MOCK_COMMON FALSE)
    set(USE_MOCK_PARALLELIZATION FALSE)
    marabou_add_test(${PARALLELIZATION_TESTS_DIR}/Test_${name} parallelization USE_MOCK_COMMON
        USE_MOCK_PARALLELIZATION "unit")
endmacro()

#parallelization_add_unit_test(DnCWorker)
#parallelization_add_unit_test(LargestIntervalDivider)

if (${BUILD_PYTHON})
    target_include_directories(${MARABOU_PY} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
