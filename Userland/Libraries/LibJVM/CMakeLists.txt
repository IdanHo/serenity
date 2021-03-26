set(SOURCES
        Class.cpp
        ClassReader.cpp
        ConstantPool.cpp
        Descriptor.cpp
        )

serenity_lib(LibJVM jvm)
target_link_libraries(LibJVM LibC)
