cmake_minimum_required(VERSION 3.25)

# vcpkg toolchain must be selected before project().
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(EXISTS "C:/Users/mpt/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "C:/Users/mpt/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "")
    elseif(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "")
    endif()
endif()

if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    if(WIN32)
        set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "")
    elseif(UNIX)
        set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING "")
    endif()
endif()

project(GetServer LANGUAGES CXX)

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

find_package(Boost CONFIG REQUIRED COMPONENTS system thread filesystem)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
find_package(redis++ CONFIG REQUIRED)

if(WIN32)
    find_package(unofficial-libmysql CONFIG REQUIRED)
    set(CHAT_MYSQL_TARGET unofficial::libmysql::libmysql)
else()
    find_package(unofficial-libmariadb CONFIG REQUIRED)
    set(CHAT_MYSQL_TARGET unofficial::libmariadb)
endif()

if(TARGET redis++::redis++)
    set(CHAT_REDIS_TARGETS redis++::redis++)
elseif(TARGET redis++::redis++_static)
    set(CHAT_REDIS_TARGETS redis++::redis++_static)
else()
    message(FATAL_ERROR "redis++ found, but no usable CMake target")
endif()

# Generate protobuf/gRPC sources when message.proto is present.
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/message.proto")
set(GENERATED_SRCS "")
if(EXISTS "${PROTO_FILE}")
    get_filename_component(PROTO_NAME "${PROTO_FILE}" NAME_WE)

    set(PB_CC "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.cc")
    set(PB_H "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.h")
    set(GRPC_CC "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.cc")
    set(GRPC_H "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.h")

    add_custom_command(
        OUTPUT ${PB_CC} ${PB_H} ${GRPC_CC} ${GRPC_H}
        COMMAND $<TARGET_FILE:protobuf::protoc>
        ARGS
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

add_executable(GetServer
    main.cpp
    const.h

    common/Singleton.h
    common/Validators.cpp
    common/Validators.h

    config/ConfigMgr.cpp
    config/ConfigMgr.h

    db/MySqlMgr.cpp
    db/MySqlMgr.h

    http/AdmissionPool.h
    http/LogicSystem.cpp
    http/LogicSystem.h
    http/PostToPool.h

    net/AsioIOServicePool.cpp
    net/AsioIOServicePool.h
    net/HttpConnection.cpp
    net/HttpConnection.h
    net/Server.cpp
    net/Server.h

    redis/RateLimiter.cpp
    redis/RateLimiter.h
    redis/RedisMgr.cpp
    redis/RedisMgr.h
    redis/VarifyCodeStore.cpp
    redis/VarifyCodeStore.h

    rpc/StatusGrpcClient.cpp
    rpc/StatusGrpcClient.h
    rpc/VarifyGrpcClient.cpp
    rpc/VarifyGrpcClient.h
    rpc/common.h

    security/PasswordUtil.cpp
    security/PasswordUtil.h

    service/LogicService.cpp
    service/LogicService.h
    service/RegisterService.cpp
    service/RegisterService.h
    service/ResetPwdService.cpp
    service/ResetPwdService.h
    service/VarifyCodeService.cpp
    service/VarifyCodeService.h

    ${GENERATED_SRCS}
)

target_include_directories(GetServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(GetServer PRIVATE
    Boost::headers
    Boost::system
    Boost::thread
    Boost::filesystem
    nlohmann_json::nlohmann_json
    gRPC::grpc++
    OpenSSL::Crypto
    OpenSSL::SSL
    ZLIB::ZLIB
    Threads::Threads
    ${CHAT_REDIS_TARGETS}
    ${CHAT_MYSQL_TARGET}
)

if(TARGET gRPC::grpc++_reflection)
    target_link_libraries(GetServer PRIVATE gRPC::grpc++_reflection)
endif()
