set(PART_NAME etrunner)

set(${PART_NAME}_INC
    convenience.hpp
    dynamic_test.hpp
    environment_dt.hpp
    test_body.hpp
)

set(${PART_NAME}_SRC
    convenience.cpp
    dynamic_test.cpp
    environment_dt.cpp
    main.cpp
    test_body.cpp
)

add_executable(${PART_NAME} ${${PART_NAME}_INC} ${${PART_NAME}_SRC})
target_compile_features(${PART_NAME} PUBLIC cxx_std_23)

target_link_libraries(${PART_NAME}
    PRIVATE
        GTest::gtest
        Boost::graph
        Boost::program_options
        Boost::filesystem
        pugixml::pugixml
        TBB::tbb
        ZLIB::ZLIB
)

install(TARGETS ${PART_NAME} DESTINATION "."
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
