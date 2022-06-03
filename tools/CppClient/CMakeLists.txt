cmake_minimum_required(VERSION 3.0)

project(HwMonitor-CppClient)

include_directories(
	include
	../../src
	../../external/CppUtils-Essential
	../../external/CppUtils-Network
)

file(GLOB SOURCE_FILES
	"src/*.hpp" "src/*.cpp"
	"../../external/CppUtils-Essential/*.hpp" "../../external/CppUtils-Essential/*.cpp"
	"../../external/CppUtils-Network/*.hpp" "../../external/CppUtils-Network/*.cpp"
)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
	add_compile_definitions(DEBUG SAFETY_CHECKS)
else()
	#add_compile_definitions(CRITICALS_CATCHABLE)
endif()

add_library(hwmoncl STATIC ${SOURCE_FILES})

install(TARGETS hwmoncl DESTINATION lib)
install(DIRECTORY include DESTINATION include)
