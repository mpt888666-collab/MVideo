cmake_minimum_required(VERSION 3.25)

# Keep the same vcpkg fallback as the other MVideo projects.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "C:/Users/mpt/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "C:/Users/mpt/vcpkg/scripts/buildsystems/vcpkg.cmake"
            CACHE FILEPATH "vcpkg toolchain")
endif()

if(NOT DEFINED VCPKG_TARGET_TRIPLET AND WIN32)
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg target triplet")
endif()

project(MVideoClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Qml
        Quick
        QuickControls2
        QuickLayouts
        Network
        Multimedia
        OpenGL
)
find_package(Threads REQUIRED)

qt_policy(SET QTP0001 NEW)

set(MVIDEO_QML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/qml")

qt_add_executable(MVideoClient
        main.cpp
        HttpMgr.cpp
        HttpMgr.h
        Singleton.h
        const.h
        AuthServer.cpp
        AuthServer.h
        AuthWork.cpp
        AuthWork.h
        UserInfo.cpp
        UserInfo.h
)

target_include_directories(MVideoClient PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(MVideoClient PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickLayouts
        Qt6::Network
        Qt6::Multimedia
        Qt6::OpenGL
        Threads::Threads
)

set(MVIDEO_QML_FILES
        qml/Main.qml
        qml/AuthScaffold.qml
        qml/AuthField.qml
        qml/AuthButton.qml
        qml/LinkText.qml
        qml/LoginPage.qml
        qml/RegisterPage.qml
        qml/ForgotPasswordPage.qml
        qml/HomePage.qml
)

foreach(qml_file IN LISTS MVIDEO_QML_FILES)
    get_filename_component(qml_alias "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_alias}")
endforeach()

qt_add_qml_module(MVideoClient
        URI MVideoClient
        VERSION 1.0
        OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml_modules"
        QML_FILES
                ${MVIDEO_QML_FILES}
)

option(MVIDEO_CLIENT_DEPLOY_QT "Deploy Qt runtime next to the executable after build" ON)

if(WIN32 AND MVIDEO_CLIENT_DEPLOY_QT AND TARGET Qt6::windeployqt)
    add_custom_command(TARGET MVideoClient POST_BUILD
            COMMAND "$<TARGET_FILE:Qt6::windeployqt>"
                    --qmldir "${MVIDEO_QML_DIR}"
                    --no-translations
                    --no-system-d3d-compiler
                    "$<TARGET_FILE:MVideoClient>"
            COMMENT "Deploying Qt Quick runtime for MVideoClient"
            VERBATIM)
endif()
