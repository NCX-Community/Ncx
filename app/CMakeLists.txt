cmake_minimum_required(VERSION 3.10)

# 设置项目名称和版本
project(ncxs VERSION 1.0)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Threads REQUIRED)

# libevent
find_package(Libevent CONFIG QUIET COMPONENTS core extra pthreads)
if(NOT Libevent_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBEVENT_CORE    REQUIRED IMPORTED_TARGET libevent_core)
    pkg_check_modules(LIBEVENT_EXTRA   REQUIRED IMPORTED_TARGET libevent_extra)
    pkg_check_modules(LIBEVENT_PTHREADS REQUIRED IMPORTED_TARGET libevent_pthreads)
    set(LIBEVENT_TARGETS
        PkgConfig::LIBEVENT_CORE
        PkgConfig::LIBEVENT_EXTRA
        PkgConfig::LIBEVENT_PTHREADS)
    message(STATUS "Using libevent via pkg-config")
else()
    set(LIBEVENT_TARGETS libevent::core libevent::extra libevent::pthreads)
    message(STATUS "Using libevent ${Libevent_VERSION} via find_package")
endif()

# protobuf
# 查找Protobuf库
find_package(Protobuf REQUIRED)
# 打印 Protobuf 信息
message(STATUS "Using Protobuf ${Protobuf_VERSION}")
message(STATUS "Protobuf include dirs: ${Protobuf_INCLUDE_DIRS}")
message(STATUS "Protobuf libraries: ${Protobuf_LIBRARIES}")

# module 
include_directories(
    cli
    util
    protocol
    network
    ncx_client
    ncx_server
)

# Boost
find_package(Boost REQUIRED COMPONENTS program_options)
if(BOOST_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    message(STATUS "Using Boost ${Boost_VERSION}")
    message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
else()
    message(FATAL_ERROR "Boost not found")
endif()

# TomlPlusPlus
include(FetchContent)
FetchContent_Declare(
    TomlPlusPlus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(TomlPlusPlus)

# source code
aux_source_directory(network NET)
aux_source_directory(util UTIL)
aux_source_directory(ncx_client NCX_CLIENT)
aux_source_directory(ncx_server NCX_SERVER)
aux_source_directory(protocol PROTOCOL)
aux_source_directory(cli CLI)

# src 
file(GLOB NCXS "ncx_server/ncxs.cpp")
file(GLOB NCXC "ncx_client/ncxc.cpp")
add_executable(ncxs ${NCXS} ${NET} ${UTIL} ${NCX_SERVER} ${PROTOCOL} ${CLI})
add_executable(ncxc ${NCXC} ${NET} ${UTIL} ${NCX_CLIENT} ${PROTOCOL} ${CLI})

# link boost
target_link_libraries(ncxc ${Boost_LIBRARIES})
target_link_libraries(ncxs ${Boost_LIBRARIES})

# link boost program_options
target_link_libraries(ncxc Boost::program_options)
target_link_libraries(ncxs Boost::program_options)

# link tomlplusplus
target_link_libraries(ncxc tomlplusplus::tomlplusplus)
target_link_libraries(ncxs tomlplusplus::tomlplusplus)

# link protobuf
target_link_libraries(ncxc ${Protobuf_LIBRARIES})
target_link_libraries(ncxs ${Protobuf_LIBRARIES})

# link libevent
target_link_libraries(ncxc ${LIBEVENT_TARGETS})
target_link_libraries(ncxs ${LIBEVENT_TARGETS})