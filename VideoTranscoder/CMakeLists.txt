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

project(VideoTranscoder LANGUAGES CXX)

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

find_package(Boost CONFIG REQUIRED COMPONENTS system thread filesystem process)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

if(WIN32)
    find_package(unofficial-libmysql CONFIG REQUIRED)
    set(TRANSCODER_MYSQL_TARGET unofficial::libmysql::libmysql)
else()
    find_package(unofficial-libmariadb CONFIG REQUIRED)
    set(TRANSCODER_MYSQL_TARGET unofficial::libmariadb)
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

set(TRANSCODER_SOURCES
    main.cpp
        Singleton.h
        ConfigMgr.h
        ConfigMgr.cpp
        MySqlMgr.cpp
        MySqlMgr.h
        EncodePoller.cpp
        EncodePoller.h
        EncodeMgr.cpp
        EncodeMgr.h
        ThreadPools.cpp
        ThreadPools.h
        EncodeWorker.cpp
        EncodeWorker.h
        const.h
        Types.cpp
        Types.h
        MinioClient.h
        FfmpegRunner.cpp
        FfmpegRunner.h
        Ladder.cpp
        Ladder.h
        FfmpegJob.cpp
        FfmpegJob.h

        # Add new files here as you create them, for example:
    # transcode/FfmpegRunner.cpp
    # transcode/FfmpegRunner.h
    # transcode/TranscodeService.cpp
    # transcode/TranscodeService.h
    # rpc/TranscodeGrpcServer.cpp
    # rpc/TranscodeGrpcServer.h
)

# Local copies of GetServer sources, flattened into the project root.
# The copied .cpp/.h must use plain #include "ConfigMgr.h" / "MinioClient.h".
set(TRANSCODER_LOCAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ConfigMgr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MinioClient.cpp
)

foreach(src IN LISTS TRANSCODER_LOCAL_SOURCES)
    if(EXISTS "${src}")
        list(APPEND TRANSCODER_SOURCES "${src}")
    else()
        message(STATUS "VideoTranscoder: not copied yet -> ${src}")
    endif()
endforeach()

add_executable(VideoTranscoder
    ${TRANSCODER_SOURCES}
    ${GENERATED_SRCS}
)

target_include_directories(VideoTranscoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(VideoTranscoder PRIVATE
    Boost::headers
    Boost::system
    Boost::thread
    Boost::filesystem
    Boost::process
    nlohmann_json::nlohmann_json
    gRPC::grpc++
    OpenSSL::Crypto
    OpenSSL::SSL
    ZLIB::ZLIB
    Threads::Threads
    ${TRANSCODER_MYSQL_TARGET}
)

if(TARGET gRPC::grpc++_reflection)
    target_link_libraries(VideoTranscoder PRIVATE gRPC::grpc++_reflection)
endif()

# ConfigMgr reads config.ini from the current working directory.
# Prefer the copy in the source tree; if only the build-tree copy exists,
# do not copy a file onto itself (cmake -E fails in that case).
set(TRANSCODER_CONFIG_INI "${CMAKE_CURRENT_SOURCE_DIR}/config.ini")
if(NOT EXISTS "${TRANSCODER_CONFIG_INI}" AND EXISTS "${CMAKE_CURRENT_BINARY_DIR}/config.ini")
    set(TRANSCODER_CONFIG_INI "${CMAKE_CURRENT_BINARY_DIR}/config.ini")
endif()
if(EXISTS "${TRANSCODER_CONFIG_INI}")
    get_filename_component(TRANSCODER_CONFIG_DIR "${TRANSCODER_CONFIG_INI}" DIRECTORY)
    if(NOT TRANSCODER_CONFIG_DIR STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
        add_custom_command(TARGET VideoTranscoder POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${TRANSCODER_CONFIG_INI}"
                "$<TARGET_FILE_DIR:VideoTranscoder>/config.ini"
            COMMENT "Copy config.ini next to VideoTranscoder.exe"
        )
    endif()
endif()