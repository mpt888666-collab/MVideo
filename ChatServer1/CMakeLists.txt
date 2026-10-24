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

project(ChatServer1 LANGUAGES CXX)

# vcpkg's x64-windows triplet is built with MSVC. MinGW cannot link those
# .lib files, so fail at configure time instead of producing a confusing
# linker error such as "unrecognized command-line option '-ignore:4221'".
if(MINGW AND VCPKG_TARGET_TRIPLET MATCHES "windows")
    message(FATAL_ERROR
        "MinGW is selected, but VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET} "
        "contains MSVC libraries. Select the Visual Studio toolchain in CLion, "
        "or install/use the x64-mingw-static triplet.")
endif()

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
find_package(nlohmann_json CONFIG REQUIRED)
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

# ChatServer persists messages, so keep MySQL available from the start.
if(WIN32)
    find_package(unofficial-libmysql CONFIG REQUIRED)
    set(CHAT_MYSQL_TARGET unofficial::libmysql::libmysql)
else()
    find_package(unofficial-libmariadb CONFIG REQUIRED)
    set(CHAT_MYSQL_TARGET unofficial::libmariadb)
endif()

# A local message.proto takes precedence. Until ChatServer has its own
# contract, reuse the shared one from GetServer so the target still builds.
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/message.proto")
if(NOT EXISTS "${PROTO_FILE}")
    set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../GetServer/message.proto")
endif()

set(GENERATED_SRCS "")
if(EXISTS "${PROTO_FILE}")
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
endif()

# Keep source groups aligned with the directory layout.
set(CHAT_SERVER_SOURCES
    main.cpp
    const.h

    common/Singleton.h
    common/ThreadPool.h

    config/ConfigMgr.cpp
    config/ConfigMgr.h

    db/MySqlMgr.cpp
    db/MySqlMgr.h

    logic/LogicNode.h
    logic/LogicSystem.cpp
    logic/LogicSystem.h

    net/AsioIOServicePool.cpp
    net/AsioIOServicePool.h
    net/CServer.cpp
    net/CServer.h
    net/CSession.cpp
    net/CSession.h
    net/MsgNode.cpp
    net/MsgNode.h

    redis/ChatRedis.h
    redis/RedisCleanup.cpp
    redis/RedisCleanup.h
    redis/RedisMgr.cpp
    redis/RedisMgr.h

    service/LoginService.cpp
    service/LoginService.h
)

add_executable(ChatServer1
    ${CHAT_SERVER_SOURCES}
    ${GENERATED_SRCS}
)

target_include_directories(ChatServer1 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(ChatServer1 PRIVATE
    Boost::headers
    Boost::system
    Boost::thread
    Boost::filesystem
    nlohmann_json::nlohmann_json
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
    ${CHAT_REDIS_TARGETS}
    ${CHAT_MYSQL_TARGET}
)

if(TARGET gRPC::grpc++_reflection)
    target_link_libraries(ChatServer1 PRIVATE gRPC::grpc++_reflection)
endif()

# Copy vcpkg runtime DLLs next to ChatServer1.exe. This is needed when the
# IDE configures without actually loading the vcpkg toolchain on the first run.
if(WIN32 AND MVCPKG_ROOT AND EXISTS "${MVCPKG_ROOT}/scripts/buildsystems/msbuild/applocal.ps1")
    add_custom_command(TARGET ChatServer1 POST_BUILD
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
            -File "${MVCPKG_ROOT}/scripts/buildsystems/msbuild/applocal.ps1"
            -targetBinary "$<TARGET_FILE:ChatServer1>"
            -installedDir "$<IF:$<CONFIG:Debug>,${MVCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/debug/bin,${MVCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/bin>"
            -OutVariable out
        VERBATIM)
endif()

# gRPC's imported targets currently resolve zlib to the release zlib.lib even
# in Debug, so the debug applocal pass misses zlib1.dll.
if(WIN32 AND MVCPKG_ROOT AND EXISTS "${MVCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/bin/zlib1.dll")
    add_custom_command(TARGET ChatServer1 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MVCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/bin/zlib1.dll"
            "$<TARGET_FILE_DIR:ChatServer1>"
        VERBATIM)
endif()
