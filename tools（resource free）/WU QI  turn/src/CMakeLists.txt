cmake_minimum_required(VERSION 3.16)
project(wq_turn LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(OpenCV REQUIRED)
add_executable(turn main.cpp)
target_link_libraries(turn PRIVATE ${OpenCV_LIBS})
add_custom_command(
    TARGET turn POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assert"
    "$<TARGET_FILE_DIR:turn>/assert"
)
add_custom_command(
   TARGET turn POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy_if_different
   "${CMAKE_SOURCE_DIR}/Flandre.onnx"
   "$<TARGET_FILE_DIR:turn>"
)
if(WIN32)
string(REPLACE "/lib" "/bin" OpenCV_BIN_DIR "${OpenCV_DIR}")
       file(GLOB_RECURSE OPENCV_DLLS 
        "${OpenCV_BIN_DIR}/opencv_videoio_ffmpeg*.dll"
        "${OpenCV_BIN_DIR}/opencv_world*.dll"
    )

    if(OPENCV_DLLS)
        add_custom_command(
            TARGET turn POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${OPENCV_DLLS}
                "$<TARGET_FILE_DIR:turn>"
            COMMENT "Automatically copying OpenCV DLLs..."
        )
    else()
        message(WARNING "IN ${OpenCV_DIR} don't find any OpenCV DLL！")
    endif()
endif()