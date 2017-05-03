cmake_minimum_required(VERSION 3.7.1)
project(qdtools)

set(
	INCLUDE_DIRECTORY
	${PROJECT_SOURCE_DIR}/include
)

set(
	SOURCE_DIRECTORY
	${PROJECT_SOURCE_DIR}/source
)

set(
	MAIN_DIRECTORY
	${PROJECT_SOURCE_DIR}/main
)

add_subdirectory(
	main-cmake-lists/qddifference
)

add_subdirectory(
	main-cmake-lists/qdversionchange
)
