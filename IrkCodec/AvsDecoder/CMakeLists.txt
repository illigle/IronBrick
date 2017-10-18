cmake_minimum_required(VERSION 3.5)

set(THIS_LIB IrkAvsDecoder)
project(${THIS_LIB})

set(INC_DIR 
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/utility"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include/codec"
)

set(INC_FILES 
    ${INC_DIR}/IrkAvsDecoder.h
    ${INC_DIR}/IrkCodec.h
)
set(SRC_FILES 
    AvsDecoder.h
    AvsDecoder.cpp
    AvsCheckCpu.cpp
    AvsDecUtility.h
    AvsBitstream.h
    AvsHeaders.h
    AvsHeaders.cpp
    AvsVlcParser.h
    AvsVlcParser.cpp
    AvsAecParser.h
    AvsAecParser.cpp
    AvsIntraPred.h
    AvsIntraPred.cpp
    AvsInterPred.h
    AvsInterPred.cpp
    AvsSlice.cpp
    AvsMacroblock.cpp
    AvsLoopFilter.cpp
    AvsIdct.cpp
)

add_library(${THIS_LIB} SHARED ${INC_FILES} ${SRC_FILES})
target_include_directories(${THIS_LIB} PRIVATE ${INC_DIR})

if(MSVC)
target_compile_definitions(${THIS_LIB} PUBLIC _CRT_SECURE_NO_DEPRECATE)
target_compile_options(${THIS_LIB} PRIVATE /GR-)
else()
target_compile_definitions(${THIS_LIB} PUBLIC _FILE_OFFSET_BITS=64)
target_compile_options(${THIS_LIB} PUBLIC -std=c++14)
target_compile_options(${THIS_LIB} PRIVATE -msse4)
endif()
target_compile_definitions(${THIS_LIB} PRIVATE IRK_AVSDECLIB_IMPL) # dll exports

# depending on IrkUtility
add_dependencies(${THIS_LIB} IrkUtility)
target_link_libraries(${THIS_LIB} IrkUtility)

if(UNIX)
target_link_libraries(${THIS_LIB} pthread)
endif()

set_target_properties(${THIS_LIB} PROPERTIES DEBUG_POSTFIX "D")

# library output dir
set_target_properties(${THIS_LIB} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set_target_properties(${THIS_LIB} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib")
set_target_properties(${THIS_LIB} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib")
set_target_properties(${THIS_LIB} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set_target_properties(${THIS_LIB} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib")
set_target_properties(${THIS_LIB} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib")
set_target_properties(${THIS_LIB} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(${THIS_LIB} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")
set_target_properties(${THIS_LIB} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")

