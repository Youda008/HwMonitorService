cmake_minimum_required(VERSION 3.13)

project(HwMonitor-CppClient)

# the main output of this project
add_library(hwmoncl STATIC)

# add this include directory to everyone who imports this library using target_link_libraries()
target_include_directories(hwmoncl PUBLIC include)

# add all local source files to this project, but not to the project of those who import this library
file(GLOB SrcFiles CONFIGURE_DEPENDS "src/*.hpp" "src/*.cpp")
target_sources(hwmoncl PRIVATE ${SrcFiles})

# get source files and compiler options of these submodules
add_subdirectory(../../external/CppUtils-Essential external/CppUtils-Essential)
add_subdirectory(../../external/CppUtils-Network external/CppUtils-Network)

# build cpp utils submodules
if(CPP_UTILS MATCHES "separate")
	# build them as separate libraries - required when multiple static libraries use cpp utils
	message("Building CppUtils as separate static libraries")
	
	add_library(cppbase STATIC)
	target_include_directories(cppbase PUBLIC ${CppEssential_IncludeDirs})
	target_sources(cppbase PRIVATE ${CppEssential_SrcFiles})
	target_compile_definitions(cppbase PUBLIC ${CppEssential_CompDefs})
	target_link_libraries(cppbase ${CppEssential_LinkedLibs})
	
	add_library(cppnet STATIC)
	target_include_directories(cppnet PUBLIC ${CppNetwork_IncludeDirs})
	target_sources(cppnet PRIVATE ${CppNetwork_SrcFiles})
	target_compile_definitions(cppnet PUBLIC ${CppNetwork_CompDefs})
	target_link_libraries(cppnet ${CppNetwork_LinkedLibs})
	
	target_link_libraries(hwmoncl cppnet cppbase)
else()
	# build them as a private part this library (default)
	message("Building CppUtils as a part of this library")
	
	target_include_directories(hwmoncl PRIVATE ${CppEssential_IncludeDirs})
	target_sources(hwmoncl PRIVATE ${CppEssential_SrcFiles})
	target_compile_definitions(hwmoncl PRIVATE ${CppEssential_CompDefs})
	target_link_libraries(hwmoncl ${CppEssential_LinkedLibs})
	
	target_include_directories(hwmoncl PRIVATE ${CppNetwork_IncludeDirs})
	target_sources(hwmoncl PRIVATE ${CppNetwork_SrcFiles})
	target_compile_definitions(hwmoncl PRIVATE ${CppNetwork_CompDefs})
	target_link_libraries(hwmoncl ${CppNetwork_LinkedLibs})
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
	# add these defitions to all targets in this file
	add_compile_definitions(DEBUG)
endif()

# install the static library file to lib and copy the whole include directory
install(TARGETS hwmoncl DESTINATION lib)
install(DIRECTORY include DESTINATION include)
