project(fs_demo)

set(SCENARIO_DIR scripts)

set(SCENARIO_FILES
        ${SCENARIO_DIR}/sample.txt
        ${SCENARIO_DIR}/sample2.txt
        )

foreach(FILE ${SCENARIO_FILES})
    configure_file(${FILE} ${FILE})
endforeach(FILE)

add_executable(fs_demo main.cpp)
target_link_libraries(fs_demo PRIVATE ${LIB_NAME})
