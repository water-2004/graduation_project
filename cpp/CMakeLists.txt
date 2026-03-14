cmake_minimum_required(VERSION 3.20)
project(edge_infer_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ONNXRUNTIME_ROOT "" CACHE PATH "ONNX Runtime C/C++ prebuilt root")
if(NOT ONNXRUNTIME_ROOT AND DEFINED ENV{ONNXRUNTIME_ROOT})
    set(ONNXRUNTIME_ROOT "$ENV{ONNXRUNTIME_ROOT}")
endif()

if(NOT ONNXRUNTIME_ROOT)
    message(FATAL_ERROR "Please pass -DONNXRUNTIME_ROOT=...")
endif()

set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_ROOT}/include")
if(NOT EXISTS "${ONNXRUNTIME_INCLUDE_DIR}/onnxruntime_cxx_api.h")
    message(FATAL_ERROR "Header not found: ${ONNXRUNTIME_INCLUDE_DIR}/onnxruntime_cxx_api.h")
endif()

if(WIN32)
    set(ONNXRUNTIME_LIB "${ONNXRUNTIME_ROOT}/lib/onnxruntime.lib")
    if(EXISTS "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll")
        set(ONNXRUNTIME_DLL "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll")
    elseif(EXISTS "${ONNXRUNTIME_ROOT}/bin/onnxruntime.dll")
        set(ONNXRUNTIME_DLL "${ONNXRUNTIME_ROOT}/bin/onnxruntime.dll")
    endif()
elseif(APPLE)
    set(ONNXRUNTIME_LIB "${ONNXRUNTIME_ROOT}/lib/libonnxruntime.dylib")
elseif(UNIX)
    set(ONNXRUNTIME_LIB "${ONNXRUNTIME_ROOT}/lib/libonnxruntime.so")
endif()

if(NOT EXISTS "${ONNXRUNTIME_LIB}")
    message(FATAL_ERROR "ONNX Runtime library not found: ${ONNXRUNTIME_LIB}")
endif()

set(BOOST_ROOT "" CACHE PATH "Boost root (e.g. F:/boost_1_87_0)")
set(BOOST_INCLUDE_DIR "")
if(BOOST_ROOT)
    if(EXISTS "${BOOST_ROOT}/include/boost-1_87/boost/asio.hpp")
        set(BOOST_INCLUDE_DIR "${BOOST_ROOT}/include/boost-1_87")
    elseif(EXISTS "${BOOST_ROOT}/include/boost/asio.hpp")
        set(BOOST_INCLUDE_DIR "${BOOST_ROOT}/include")
    endif()
endif()

set(SHARED_LOGGER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../shared")
set(SHARED_LOGGER_SOURCES
    "${SHARED_LOGGER_DIR}/logger.cpp"
)

add_executable(edge_infer
    src/edge_infer.cpp
    ${SHARED_LOGGER_SOURCES}
)
target_include_directories(edge_infer PRIVATE "${ONNXRUNTIME_INCLUDE_DIR}" "${SHARED_LOGGER_DIR}")
target_link_libraries(edge_infer PRIVATE "${ONNXRUNTIME_LIB}")

if(MSVC)
    target_compile_options(edge_infer PRIVATE /W4 /utf-8)
else()
    target_compile_options(edge_infer PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32 AND DEFINED ONNXRUNTIME_DLL AND EXISTS "${ONNXRUNTIME_DLL}")
    add_custom_command(
        TARGET edge_infer
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_DLL}"
            "$<TARGET_FILE_DIR:edge_infer>/onnxruntime.dll"
    )
endif()

if(BOOST_INCLUDE_DIR)
    add_executable(
        edge_infer_service
        src/edge_infer_service.cpp
        src/net/io_context_pool.cpp
        src/net/tcp_server.cpp
        src/net/tcp_session.cpp
        src/logic/logic_system.cpp
        src/service/inference_engine.cpp
        ${SHARED_LOGGER_SOURCES}
    )

    target_include_directories(
        edge_infer_service
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${ONNXRUNTIME_INCLUDE_DIR}"
            "${BOOST_INCLUDE_DIR}"
            "${SHARED_LOGGER_DIR}"
    )
    target_link_libraries(edge_infer_service PRIVATE "${ONNXRUNTIME_LIB}")
    target_compile_definitions(edge_infer_service PRIVATE BOOST_ERROR_CODE_HEADER_ONLY)

    if(WIN32)
        target_link_libraries(edge_infer_service PRIVATE ws2_32 mswsock)
        target_compile_definitions(edge_infer_service PRIVATE _WIN32_WINNT=0x0A00)
    endif()

    if(MSVC)
        target_compile_options(edge_infer_service PRIVATE /W4 /utf-8)
    else()
        target_compile_options(edge_infer_service PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(WIN32 AND DEFINED ONNXRUNTIME_DLL AND EXISTS "${ONNXRUNTIME_DLL}")
        add_custom_command(
            TARGET edge_infer_service
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ONNXRUNTIME_DLL}"
                "$<TARGET_FILE_DIR:edge_infer_service>/onnxruntime.dll"
        )
    endif()
else()
    message(WARNING "Boost.Asio headers not found. edge_infer_service target will be skipped.")
endif()
