cmake_minimum_required(VERSION 3.5)

set(THIS_LIB IrkNetwork)
project(${THIS_LIB})

set(INC_DIR 
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/utility"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/network"
)

set(INC_FILES 
    ${INC_DIR}/IrkIpAddress.h
    ${INC_DIR}/IrkURI.h
    ${INC_DIR}/IrkSocketError.h
    ${INC_DIR}/IrkSocket.h
    ${INC_DIR}/IrkDnsResolver.h
    ${INC_DIR}/IrkHttpUtility.h
)
set(SRC_FILES 
    src/IrkIpAddress.cpp
    src/IrkURI.cpp
    src/IrkSocket.cpp
    src/IrkHttpUtility.cpp
)

add_library(${THIS_LIB} STATIC ${INC_FILES} ${SRC_FILES})

# depending on IrkUtility
add_dependencies(${THIS_LIB} IrkUtility)

target_include_directories(${THIS_LIB} PRIVATE ${INC_DIR})

if(MSVC)
target_compile_definitions(${THIS_LIB} PUBLIC _CRT_SECURE_NO_DEPRECATE)
target_compile_options(${THIS_LIB} PRIVATE /GR-)
else()
target_compile_definitions(${THIS_LIB} PUBLIC _FILE_OFFSET_BITS=64)
target_compile_options(${THIS_LIB} PUBLIC -std=c++14)
endif()

set_target_properties(${THIS_LIB} PROPERTIES DEBUG_POSTFIX "D")

# library output dir
set_target_properties(${THIS_LIB} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set_target_properties(${THIS_LIB} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib")
set_target_properties(${THIS_LIB} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib")

###############################################################################
add_executable(EchoServer test/echo_server/EchoServer.cpp)

target_include_directories(EchoServer PRIVATE ${INC_DIR})

target_link_libraries(EchoServer ${THIS_LIB})
if(WIN32)
target_link_libraries(EchoServer Ws2_32 WinHttp)
else()
target_link_libraries(EchoServer pthread)
endif()

set_target_properties(EchoServer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(EchoServer PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")
set_target_properties(EchoServer PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")

###############################################################################
set(TEST_EXE test_irknet)

set(TEST_FILES 
    test/main.cpp    
    test/test_addr.cpp
    test/test_uri.cpp
    test/test_resolver.cpp
    test/test_socket.cpp
)

add_executable(${TEST_EXE} ${TEST_FILES})

add_dependencies(${TEST_EXE} ${THIS_LIB} EchoServer)

# include dir
if(MSVC)
target_include_directories(${TEST_EXE} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../external/include")
endif()
target_include_directories(${TEST_EXE} PRIVATE ${INC_DIR})

# link IrkUtility and IrkNetwork library
target_link_libraries(${TEST_EXE} ${THIS_LIB})
target_link_libraries(${TEST_EXE} IrkUtility)

# find and link gtest
if(MSVC)
set(EXTERNAL_LIB "${CMAKE_CURRENT_SOURCE_DIR}/../external/lib")
find_library(GTEST_DEBUG gtestD ${EXTERNAL_LIB})
find_library(GTEST gtest ${EXTERNAL_LIB})
target_link_libraries(${TEST_EXE} debug ${GTEST_DEBUG})
target_link_libraries(${TEST_EXE} optimized ${GTEST})
else()
find_library(GTEST gtest)
target_link_libraries(${TEST_EXE} ${GTEST})
endif()

# other link libraries
if(WIN32)
target_link_libraries(${TEST_EXE} Ws2_32 WinHttp)
else()
target_link_libraries(${TEST_EXE} pthread)
endif()

set_target_properties(${TEST_EXE} PROPERTIES DEBUG_POSTFIX "D")

# output dir
set_target_properties(${TEST_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(${TEST_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")
set_target_properties(${TEST_EXE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
