# Parent project does not export its library target, so this CML implicitly
# depends on being added from it, i.e. the testing is done only from the build
# tree and is not feasible from an install location

project(ChemisKitTests LANGUAGES CXX)

# ---- Tests ----

add_executable(ChemisKit_test source/ChemisKit_test.cpp)
target_link_libraries(ChemisKit_test PRIVATE ChemisKit_lib)
target_compile_features(ChemisKit_test PRIVATE cxx_std_17)

add_test(NAME ChemisKit_test COMMAND ChemisKit_test)

# ---- End-of-file commands ----

add_folders(Test)
