
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -ffast-math -Wall -Wextra -Wno-unused-parameter")


cmake_minimum_required (VERSION 2.6)
project (gray_cl)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
       "Choose the type of build, options are: Debug Release"
       FORCE)
endif(NOT CMAKE_BUILD_TYPE)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
        message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLUT REQUIRED)
find_package(OpenCL REQUIRED)
find_package(PNG REQUIRED)

include_directories(${PROJECT_BINARY_DIR} ${OPENGL_INCLUDE_DIRS}  ${GLUT_INCLUDE_DIRS} ${PNG_INCLUDE_DIR} ${GLEW_INCLUDE_DIRS} ${OpenCL_INCLUDE_DIRS})

	
add_executable(gray_cl cl_gl.cpp gl_renderer.cpp gray_cl.cpp image.cpp)
target_link_libraries (gray_cl ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${PNG_LIBRARY} ${GLEW_LIBRARIES} ${OpenCL_LIBRARIES})





