set(TEST_SOURCES
    TestBrotli.cpp
    TestDeflate.cpp
    TestGzip.cpp
    TestHuffman.cpp
    TestZlib.cpp
)

foreach(source IN LISTS TEST_SOURCES)
    serenity_test("${source}" LibCompress LIBS LibCompress)
endforeach()
