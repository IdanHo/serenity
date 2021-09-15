set(SOURCES
    Deflate.cpp
    Gzip.cpp
    Huffman.cpp
    Zlib.cpp
)

serenity_lib(LibCompress compress)
target_link_libraries(LibCompress LibC LibCrypto)
