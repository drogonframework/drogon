LINK_LIBRARIES(drogon trantor pthread dl)

ADD_EXECUTABLE(cache_map_test CacheMapTest.cc)
ADD_EXECUTABLE(cache_map_test2 CacheMapTest2.cc)
ADD_EXECUTABLE(cookies_test CookiesTest.cc)
ADD_EXECUTABLE(class_name_test ClassNameTest.cc)
ADD_EXECUTABLE(sha1_test Sha1Test.cc)
ADD_EXECUTABLE(view_data_test HttpViewDataTest.cc)
ADD_EXECUTABLE(md5_test Md5Test.cc ../src/ssl_funcs/Md5.cc)
ADD_EXECUTABLE(http_full_date_test HttpFullDateTest.cc)
ADD_EXECUTABLE(gzip_test GzipTest.cc)
ADD_EXECUTABLE(url_codec_test UrlCodecTest.cc)
ADD_EXECUTABLE(main_loop_test MainLoopTest.cc)

SET(test_targets cache_map_test 
                cache_map_test2 
                cookies_test 
                class_name_test 
                sha1_test 
                view_data_test 
                md5_test
                http_full_date_test
                gzip_test
                url_codec_test
                main_loop_test)

SET_PROPERTY(TARGET ${test_targets} PROPERTY CXX_STANDARD ${DROGON_CXX_STANDARD})
SET_PROPERTY(TARGET ${test_targets} PROPERTY CXX_STANDARD_REQUIRED ON)
SET_PROPERTY(TARGET ${test_targets} PROPERTY CXX_EXTENSIONS OFF)