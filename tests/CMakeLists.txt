include(ECMAddTests)

ecm_add_test(test_rawimage.cpp
    TEST_NAME test_rawimage
    LINK_LIBRARIES textract_lib Qt6::Test Qt6::Gui
)

ecm_add_test(test_geometry.cpp
    TEST_NAME test_geometry
    LINK_LIBRARIES textract_lib Qt6::Test Qt6::Gui
)

ecm_add_test(test_preprocess.cpp
    TEST_NAME test_preprocess
    LINK_LIBRARIES textract_lib Qt6::Test Qt6::Gui
)

ecm_add_test(test_tesseractengine.cpp
    TEST_NAME test_tesseractengine
    LINK_LIBRARIES textract_lib Qt6::Test Qt6::Gui
)

ecm_add_test(test_assemble.cpp
    TEST_NAME test_assemble
    LINK_LIBRARIES textract_lib Qt6::Test Qt6::Gui
)

ecm_add_test(test_analyze.cpp
    TEST_NAME test_analyze
    LINK_LIBRARIES textract_lib Qt6::Test Qt6::Gui
)

ecm_add_test(test_extraction.cpp
    TEST_NAME test_extraction
    LINK_LIBRARIES textract_lib Qt6::Test Qt6::Gui
)

# Only the authorisation message, which is pure. Capturing itself still needs a
# live compositor and stays untested.
ecm_add_test(test_captureerror.cpp
    TEST_NAME test_captureerror
    LINK_LIBRARIES textract_lib Qt6::Test Qt6::Gui
)

# Corpus loader and scorer, shared by the ctest gate and the report tool.
add_library(textract_fixtures STATIC fixturecorpus.cpp)
target_include_directories(textract_fixtures PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(textract_fixtures PUBLIC textract_lib Qt6::Core)

ecm_add_test(test_fixturecorpus.cpp
    TEST_NAME test_fixturecorpus
    LINK_LIBRARIES textract_fixtures Qt6::Test Qt6::Gui
)

ecm_add_test(test_fixtures.cpp
    TEST_NAME test_fixtures
    LINK_LIBRARIES textract_fixtures Qt6::Test Qt6::Gui
)
target_compile_definitions(test_fixtures PRIVATE
    TEXTRACT_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# Not a test: prints per-fixture scores across upscale factors, so the spec's
# 3x default can be settled by measurement.
add_executable(textract-fixture-report fixturereport.cpp)
target_link_libraries(textract-fixture-report PRIVATE textract_fixtures Qt6::Gui)
target_compile_definitions(textract-fixture-report PRIVATE
    TEXTRACT_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
