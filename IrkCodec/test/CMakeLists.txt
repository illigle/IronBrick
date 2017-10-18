cmake_minimum_required(VERSION 3.5)

set(TEST_EXE test_codec)
project(${TEST_EXE})

set(INC_DIR 
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/utility"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/codec"
)

set(TEST_FILES 
    test_main.cpp    
    test_avsdecoder.cpp
    AvsFileReader.h
    AvsFileReader.cpp
)

add_executable(${TEST_EXE} ${TEST_FILES})

target_include_directories(${TEST_EXE} PRIVATE ${INC_DIR})

add_dependencies(${TEST_EXE} IrkUtility)
add_dependencies(${TEST_EXE} IrkAvsDecoder)

target_link_libraries(${TEST_EXE} IrkUtility)
target_link_libraries(${TEST_EXE} IrkAvsDecoder)

set_target_properties(${TEST_EXE} PROPERTIES DEBUG_POSTFIX "D")

# output dir
set_target_properties(${TEST_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(${TEST_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")
set_target_properties(${TEST_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
