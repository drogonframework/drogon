link_libraries(${PROJECT_NAME})
set(UNITTEST_SOURCES unittests/main.cpp
                        unittests/Base64Test.cpp
                        unittests/UrlCodecTest.cpp
                        unittests/GzipTest.cpp
                        unittests/HttpViewDataTest.cpp
                        unittests/CookieTest.cpp)

if(DROGON_CXX_STANDARD GREATER_EQUAL 20 AND HAS_COROUTINE)
    set(UNITTEST_SOURCES ${UNITTEST_SOURCES} unittests/CoroutineTest.cpp)
endif()

add_executable(unittest ${UNITTEST_SOURCES})

set(tests unittest)
set_property(TARGET ${tests}
             PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
set_property(TARGET ${tests} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${tests} PROPERTY CXX_EXTENSIONS OFF)