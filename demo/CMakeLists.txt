project(fs_demo)

set(SCENARIO_DIR scripts)

file(GLOB SCENARIO_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SCENARIO_DIR}/*)

foreach(FILE ${SCENARIO_FILES})
    configure_file(${FILE} ${FILE})
endforeach(FILE)

add_executable(fs_demo main.cpp)
target_link_libraries(fs_demo PRIVATE ${LIB_NAME})
