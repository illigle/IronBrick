cmake_minimum_required(VERSION 3.5)

project(IronBrick)

enable_testing()

add_subdirectory(IrkUtility)

add_subdirectory(IrkNetwork)

add_subdirectory(IrkCodec/AvsDecoder)
add_subdirectory(IrkCodec/test)