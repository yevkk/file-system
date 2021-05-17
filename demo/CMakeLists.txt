project(fs_demo)

set(SCENARIO_DIR scripts)

#remove .fs files from previous run
file(GLOB DEMO_FS_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/*.fs)

foreach(FILE ${DEMO_FS_FILES})
    file(REMOVE ${FILE})
endforeach(FILE)

#copy scenario files to binary dir
file(GLOB SCENARIO_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SCENARIO_DIR}/*)

foreach(FILE ${SCENARIO_FILES})
    configure_file(${FILE} ${FILE})
endforeach(FILE)

add_executable(fs_demo main.cpp)
target_link_libraries(fs_demo PRIVATE ${LIB_NAME})
