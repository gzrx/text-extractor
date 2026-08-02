set(TEXTRACT_LIB_SOURCES
    capture/rawimage.cpp
    capture/kwincapture.cpp
)

add_library(textract_lib STATIC ${TEXTRACT_LIB_SOURCES})
target_include_directories(textract_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(textract_lib PUBLIC Qt6::Core Qt6::Gui Qt6::DBus)

add_executable(textract main.cpp)
target_link_libraries(textract PRIVATE textract_lib Qt6::Widgets)

install(TARGETS textract ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES ${CMAKE_SOURCE_DIR}/data/org.kde.textract.desktop
        DESTINATION ${KDE_INSTALL_APPDIR})
