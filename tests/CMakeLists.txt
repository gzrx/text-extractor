include(ECMAddTests)

ecm_add_test(test_rawimage.cpp
    TEST_NAME test_rawimage
    LINK_LIBRARIES textract_lib Qt6::Test Qt6::Gui
)

ecm_add_test(test_geometry.cpp
    TEST_NAME test_geometry
    LINK_LIBRARIES textract_lib Qt6::Test Qt6::Gui
)

ecm_add_test(test_tesseractengine.cpp
    TEST_NAME test_tesseractengine
    LINK_LIBRARIES textract_lib Qt6::Test Qt6::Gui
)
