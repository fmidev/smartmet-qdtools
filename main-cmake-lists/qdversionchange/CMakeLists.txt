cmake_minimum_required(VERSION 3.7.1)
project(qdversionchange)

file( 
GLOB CPP_FILES 
${MAIN_DIRECTORY}/qdversionchange.cpp
 )

link_directories(
    ${BOOST_LIB_DIR}
)

add_executable( 
    ${PROJECT_NAME}
    ${CPP_FILES} 
    )

target_link_libraries(
    ${PROJECT_NAME}
    Newbase
    )

target_include_directories( 
    ${PROJECT_NAME}
    PUBLIC ${Newbase_INCLUDE_DIRS}
    PUBLIC ${BOOST_INCLUDE_DIR}
    )
