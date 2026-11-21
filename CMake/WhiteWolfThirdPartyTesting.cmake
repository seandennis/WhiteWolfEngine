# Third-party dependencies used only by the tests.
#
# Every dependency is pinned to an exact commit; the release tag it corresponds to is noted
# beside it. Record each dependency and the reason it was chosen in docs/DECISIONS.md.

include(FetchContent)

# doctest: unit test framework (MIT). Release v2.5.3.
set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG 2d0a9359a60c51affe2a9bebb1be1dca47868151
    SYSTEM)
FetchContent_MakeAvailable(doctest)
