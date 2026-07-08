cmake_minimum_required(VERSION 3.15)
project(WC3Helper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
if(MSVC)
    # MSVC特定选项
	# add_definitions(-DPOINTER_64=__ptr64)
	# add_definitions(-DPVOID64=void*__ptr64)
	# add_definitions(-D_PVOID64_DEFINED)
	# add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	# add_definitions(-D_USING_V110_SDK71_)
    add_compile_options(/W4 /WX- /utf-8)
	add_compile_definitions(WIN32_LEAN_AND_MEAN UNICODE _UNICODE SPDLOG_USE_STD_FORMAT)
	add_compile_definitions(WC3HELPER_LIBRARY _CRT_SECURE_NO_WARNINGS)
	add_compile_definitions(WC3HELPER_BASIC)
	# 打开下面这行，开启静态链接运行时库
	# set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    # GCC/Clang选项
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 包含目录
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(include)
include_directories(./)

link_directories(${CMAKE_HOME_DIRECTORY}/libs)

# 主程序源文件
set(MAIN_SOURCES
	helper.cpp
	jass.cpp
	plugins/mana.cpp
	plugins/widescreen.cpp
	plugins/unitinfo.cpp
	plugins/wfe.cpp
	plugins/ChatCommand.cpp
	plugins/unitcommand.cpp
	plugins/CoolDownDraw.cpp
	plugins/CoolDownDrawD3D9.cpp
)

# 创建WC3Helper DLL
add_library(WC3Helper SHARED ${MAIN_SOURCES})

# 告诉使用者：要包含这个目录才能用
target_include_directories(WC3Helper PUBLIC
	${CMAKE_HOME_DIRECTORY}/Detours
	${CMAKE_HOME_DIRECTORY}/dx8sdk
)

target_link_libraries(WC3Helper PRIVATE
	Version 
	detours
	d3d8
    d3dx8
	d3d9
	d3dx9
	opengl32
	freetype28
	legacy_stdio_definitions.lib
)

# set_target_properties(WC3Helper PROPERTIES SUFFIX ".mix")
# strom80
# 设置输出目录
set_target_properties(WC3Helper PROPERTIES
	LINK_FLAGS "/SAFESEH:NO"
    # RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/bin
	# RUNTIME_OUTPUT_DIRECTORY "D:/Games/Warcraft III/"
	# RUNTIME_OUTPUT_DIRECTORY_DEBUG   "E:/games/Warcraft III/"
	# RUNTIME_OUTPUT_DIRECTORY_RELEASE "E:/games/Warcraft III/"
	SUFFIX ".mix"
	OUTPUT_NAME "strom80"
)
