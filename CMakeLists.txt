cmake_minimum_required(VERSION 3.7.1)
project(qdtools)

set(
	INCLUDE_DIRECTORY
	include
)

set(
	SOURCE_DIRECTORY
	source
)

set(
	MAIN_DIRECTORY
	main
)

add_subdirectory(
	main-cmake-lists/qdversionchange
)
