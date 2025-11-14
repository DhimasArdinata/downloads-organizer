# Create the test executable, now including our own main function.
add_executable(organizer_tests
    TestMain.cpp
    RuleEngineTests.cpp
)

# Link the test executable against our core logic library and GoogleTest.
# We only need GTest::gtest since we are providing our own main().
target_link_libraries(organizer_tests PRIVATE
    organizer_lib  # Our core logic library
    GTest::gtest   # The core GoogleTest framework
)

# Discover the tests in the executable.
include(GoogleTest)
gtest_discover_tests(organizer_tests)