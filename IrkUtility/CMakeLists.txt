cmake_minimum_required(VERSION 3.5)

set(LIB_NAME IrkUtility)
project(${LIB_NAME})

set(INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../include/utility")
set(INC_FILES 
    ${INC_DIR}/IrkCommon.h
    ${INC_DIR}/IrkAtomic.h
    ${INC_DIR}/IrkSpinMutex.h
    ${INC_DIR}/IrkSyncUtility.h
    ${INC_DIR}/IrkSyncQueue.h
    ${INC_DIR}/IrkThread.h
    ${INC_DIR}/IrkThreadPool.h
    ${INC_DIR}/IrkCFile.h
    ${INC_DIR}/IrkFileWalker.h
    ${INC_DIR}/IrkDirWalker.h
    ${INC_DIR}/IrkCType.h
    ${INC_DIR}/IrkCiString.h
    ${INC_DIR}/IrkStringUtility.h   
    ${INC_DIR}/IrkEnvUtility.h
    ${INC_DIR}/IrkFlatMap.h
    ${INC_DIR}/IrkQueue.h
    ${INC_DIR}/IrkVector.h
    ${INC_DIR}/IrkCryptoHash.h
    ${INC_DIR}/IrkBase64.h
    ${INC_DIR}/IrkHMAC.h
    ${INC_DIR}/IrkCmdLine.h
    ${INC_DIR}/IrkProcess.h
    ${INC_DIR}/IrkIPC.h
    ${INC_DIR}/IrkContract.h
    ${INC_DIR}/IrkLogger.h
    ${INC_DIR}/IrkOnExit.h
    ${INC_DIR}/IrkRefCntObj.h
    ${INC_DIR}/IrkRecycleBin.h
    ${INC_DIR}/IrkMemPool.h
    ${INC_DIR}/IrkMemUtility.h
    ${INC_DIR}/IrkAllocator.h  
    ${INC_DIR}/IrkBitsUtility.h
    ${INC_DIR}/IrkRational.h
    ${INC_DIR}/IrkResult.h
    ${INC_DIR}/IrkValBox.h
    ${INC_DIR}/IrkIniFile.h
    ${INC_DIR}/IrkJSON.h
    ${INC_DIR}/IrkXML.h
)
set(SRC_FILES 
    src/IrkBitsUtility.cpp
    src/IrkEnvUtility.cpp
    src/IrkFileWalker.cpp
    src/IrkDirWalker.cpp
    src/IrkContract.cpp
    src/IrkLogger.cpp
    src/IrkMemPool.cpp
    src/IrkCryptoHash.cpp
    src/IrkBase64.cpp
    src/IrkHMAC.cpp
    src/IrkCmdLine.cpp
    src/IrkProcess.cpp
    src/IrkIPC.cpp
    src/IrkSpinMutex.cpp
    src/IrkSyncUtility.cpp
    src/IrkThread.cpp
    src/IrkThreadPool.cpp
    src/IrkStringUtility.cpp
    src/IrkIniFile.cpp
    src/IrkJSON.cpp
    src/IrkXML.cpp
)

add_library(${LIB_NAME} STATIC ${INC_FILES} ${SRC_FILES})

target_include_directories(${LIB_NAME} PRIVATE ${INC_DIR})

if(MSVC)
target_compile_definitions(${LIB_NAME} PUBLIC _CRT_SECURE_NO_DEPRECATE)
target_compile_options(${LIB_NAME} PRIVATE /GR-)
else()
target_compile_definitions(${LIB_NAME} PUBLIC _FILE_OFFSET_BITS=64)
target_compile_options(${LIB_NAME} PUBLIC -std=c++14)
endif()

set_target_properties(${LIB_NAME} PROPERTIES DEBUG_POSTFIX "D")

# library output dir
set_target_properties(${LIB_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set_target_properties(${LIB_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib")
set_target_properties(${LIB_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib")

###############################################################################
add_executable(IPCTest test/ipc_server/main.cpp)

target_include_directories(IPCTest PRIVATE ${INC_DIR})

target_link_libraries(IPCTest ${LIB_NAME})
if(NOT WIN32)
target_link_libraries(IPCTest pthread)
if(NOT APPLE)
target_link_libraries(IPCTest rt)
endif()
endif()

set_target_properties(IPCTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(IPCTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")
set_target_properties(IPCTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")

###############################################################################
set(TEST_NAME test_irkutility)

set(TEST_FILES 
    test/main.cpp    
    test/test_logger.cpp
    test/test_contract.cpp
    test/test_onexit.cpp
    test/test_rational.cpp
    test/test_refcnt.cpp
    test/test_recyclebin.cpp
    test/test_bitsutility.cpp
    test/test_atomic.cpp
    test/test_spinmutex.cpp
    test/test_syncutility.cpp
    test/test_syncqueue.cpp
    test/test_threadpool.cpp
    test/test_vector.cpp
    test/test_queue.cpp
    test/test_flatmap.cpp
    test/test_cfile.cpp
    test/test_dirwalker.cpp
    test/test_base64.cpp
    test/test_md5.cpp
    test/test_sha.cpp
    test/test_hmac.cpp
    test/test_ctype.cpp
    test/test_strutility.cpp
    test/test_cistring.cpp
    test/test_memutility.cpp
    test/test_mempool.cpp
    test/test_allocator.cpp
    test/test_valbox.cpp
    test/test_result.cpp
    test/test_ini.cpp
    test/test_json.cpp
    test/test_xml.cpp
    test/test_envutility.cpp
    test/test_cmdline.cpp
    test/test_ipc.cpp
)

add_executable(${TEST_NAME} ${TEST_FILES})

add_dependencies(${TEST_NAME} ${LIB_NAME} ${IPC_TEST})

target_compile_definitions(${TEST_NAME} PRIVATE IRK_ENABLE_CONTRACTS)

# include dir
if(MSVC)
set(EXTERNAL_INC "${CMAKE_CURRENT_SOURCE_DIR}/../external/include")
target_include_directories(${TEST_NAME} PRIVATE ${EXTERNAL_INC})
endif()
target_include_directories(${TEST_NAME} PRIVATE ${INC_DIR})

# find and link gtest, openssl
if(MSVC)
set(EXTERNAL_LIB "${CMAKE_CURRENT_SOURCE_DIR}/../external/lib")
find_library(GTEST_DEBUG gtestD ${EXTERNAL_LIB})
find_library(GTEST gtest ${EXTERNAL_LIB})
find_library(OPENSSL crypto ${EXTERNAL_LIB})
target_link_libraries(${TEST_NAME} debug ${GTEST_DEBUG})
target_link_libraries(${TEST_NAME} optimized ${GTEST})
target_link_libraries(${TEST_NAME} ${OPENSSL})
else()
find_library(GTEST gtest)
find_library(OPENSSL crypto)
target_link_libraries(${TEST_NAME} ${GTEST} ${OPENSSL})
endif()

# other link libraries
target_link_libraries(${TEST_NAME} ${LIB_NAME})
if(NOT WIN32)
target_link_libraries(${TEST_NAME} pthread)
if(NOT APPLE)
target_link_libraries(${TEST_NAME} rt)
endif()
endif()

set_target_properties(${TEST_NAME} PROPERTIES DEBUG_POSTFIX "D")

# output dir
set_target_properties(${TEST_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(${TEST_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")
set_target_properties(${TEST_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")

# copy openssl dll to ouput dir
if(MSVC)
add_custom_command(TARGET ${TEST_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${EXTERNAL_LIB}/libcrypto-1_1-x64.dll" "${CMAKE_BINARY_DIR}/bin/libcrypto-1_1-x64.dll")
endif()
