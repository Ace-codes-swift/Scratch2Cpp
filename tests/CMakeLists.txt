# Unit tests (no external framework; each test is a small executable).

# Runtime value semantics - compiled directly from the runtime sources, which
# do not need SDL for Value/Ops/List.
add_executable(test_runtime_values
    test_runtime_values.cpp
    ${CMAKE_SOURCE_DIR}/runtime/scratch/Value.cpp
    ${CMAKE_SOURCE_DIR}/runtime/scratch/Ops.cpp
    ${CMAKE_SOURCE_DIR}/runtime/scratch/List.cpp
)
target_include_directories(test_runtime_values PRIVATE ${CMAKE_SOURCE_DIR}/runtime)
add_test(NAME runtime_values COMMAND test_runtime_values)

# Parser + code generator on an in-memory project.json.
add_executable(test_converter test_converter.cpp)
target_link_libraries(test_converter PRIVATE s2c_core)
add_test(NAME converter COMMAND test_converter)
