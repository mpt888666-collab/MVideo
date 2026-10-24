cmake_minimum_required(VERSION 3.25)

# vcpkg root. Override with -DMVCPKG_ROOT=... or set MVCPKG_ROOT in the env.
set(_vcpkg_root "")
if(DEFINED MVCPKG_ROOT AND EXISTS "${MVCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    set(_vcpkg_root "${MVCPKG_ROOT}")
elseif(DEFINED ENV{MVCPKG_ROOT} AND EXISTS "$ENV{MVCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    set(_vcpkg_root "$ENV{MVCPKG_ROOT}")
elseif(WIN32 AND EXISTS "C:/Users/mpt/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(_vcpkg_root "C:/Users/mpt/vcpkg")
elseif(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    set(_vcpkg_root "$ENV{VCPKG_ROOT}")
elseif(UNIX AND EXISTS "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(_vcpkg_root "$ENV{HOME}/vcpkg")
endif()

if(_vcpkg_root)
    set(MVCPKG_ROOT "${_vcpkg_root}" CACHE PATH "vcpkg installation root")
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "${MVCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE FILEPATH "vcpkg toolchain")
    endif()
    if(NOT DEFINED VCPKG_TARGET_TRIPLET)
        if(WIN32)
            set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "")
        elseif(UNIX)
            set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING "")
        endif()
    endif()
endif()

project(StatusServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options("/utf-8")
endif()
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

# Keep the vcpkg prefix available even if the IDE loaded another toolchain.
if(MVCPKG_ROOT AND VCPKG_TARGET_TRIPLET AND EXISTS "${MVCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}")
    list(APPEND CMAKE_PREFIX_PATH "${MVCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}")
endif()

find_package(Boost CONFIG REQUIRED COMPONENTS system thread filesystem)
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(redis++ CONFIG REQUIRED)

if(TARGET redis++::redis++)
    set(CHAT_REDIS_TARGETS redis++::redis++)
elseif(TARGET redis++::redis++_static)
    set(CHAT_REDIS_TARGETS redis++::redis++_static)
else()
    message(FATAL_ERROR "redis++ found, but no usable CMake target")
endif()

# StatusServer has no proto of its own yet; reuse the shared contract in
# GetServer. Once a local message.proto exists it takes precedence.
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/message.proto")
if(NOT EXISTS "${PROTO_FILE}")
    set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../GetServer/message.proto")
endif()
if(NOT EXISTS "${PROTO_FILE}")
    message(FATAL_ERROR "message.proto not found in StatusServer or ../GetServer")
endif()

get_filename_component(PROTO_DIR "${PROTO_FILE}" DIRECTORY)
get_filename_component(PROTO_NAME "${PROTO_FILE}" NAME_WE)

set(PB_CC   "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.cc")
set(PB_H    "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.h")
set(GRPC_CC "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.cc")
set(GRPC_H  "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.h")

add_custom_command(
    OUTPUT ${PB_CC} ${PB_H} ${GRPC_CC} ${GRPC_H}
    COMMAND $<TARGET_FILE:protobuf::protoc>
    ARGS
        --proto_path=${PROTO_DIR}
        --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
        --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
        --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
        --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
        ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    VERBATIM
)

set(GENERATED_SRCS ${PB_CC} ${GRPC_CC})

set(STATUS_SERVER_SOURCES
    main.cpp
    ConfigMgr.cpp
    ConfigMgr.h
    StatusServerImp.cpp
    StatusServerImp.h
    RedisMgr.cpp
    RedisMgr.h
    Singleton.h
)

add_executable(StatusServer
    ${STATUS_SERVER_SOURCES}
    ${GENERATED_SRCS}
)

target_include_directories(StatusServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(StatusServer PRIVATE
    Boost::headers
    Boost::system
    Boost::thread
    Boost::filesystem
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
    ${CHAT_REDIS_TARGETS}
)

if(TARGET gRPC::grpc++_reflection)
    target_link_libraries(StatusServer PRIVATE gRPC::grpc++_reflection)
endif()
# Copy vcpkg runtime DLLs next to StatusServer.exe. This is needed when the
# IDE configures without actually loading the vcpkg toolchain on the first run.
if(WIN32 AND MVCPKG_ROOT AND EXISTS "${MVCPKG_ROOT}/scripts/buildsystems/msbuild/applocal.ps1")
    add_custom_command(TARGET StatusServer POST_BUILD
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${MVCPKG_ROOT}/scripts/buildsystems/msbuild/applocal.ps1"
            -targetBinary "$<TARGET_FILE:StatusServer>"
            -installedDir "$<IF:$<CONFIG:Debug>,${MVCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/debug/bin,${MVCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/bin>"
            -OutVariable out
        VERBATIM)
endif()

# gRPC's imported targets currently resolve zlib to the release zlib.lib even
# in Debug, so the debug applocal pass misses zlib1.dll.
if(WIN32 AND MVCPKG_ROOT AND EXISTS "${MVCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/bin/zlib1.dll")
    add_custom_command(TARGET StatusServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MVCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/bin/zlib1.dll"
            "$<TARGET_FILE_DIR:StatusServer>"
        VERBATIM)
endif()
