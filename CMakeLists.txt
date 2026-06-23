cmake_minimum_required(VERSION 3.16)

project(LiveAISignLanguageClassification
    VERSION 1.0.0
    DESCRIPTION "Live AI Sign Language Classification"
    LANGUAGES CXX
)

add_executable(LiveAISignLanguageClassification
    src/main.cpp
    src/Classifier.cpp
    src/FrameSource.cpp
)

target_include_directories(LiveAISignLanguageClassification PRIVATE include)
target_compile_features(LiveAISignLanguageClassification PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(LiveAISignLanguageClassification PRIVATE /W4 /permissive-)
else()
    target_compile_options(LiveAISignLanguageClassification PRIVATE -Wall -Wextra -pedantic)
endif()

add_custom_command(TARGET LiveAISignLanguageClassification POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/data
        $<TARGET_FILE_DIR:LiveAISignLanguageClassification>/data
)
