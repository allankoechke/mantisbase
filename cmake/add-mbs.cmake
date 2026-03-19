add_library(mbs STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/mbs/src/lexer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/mbs/src/parser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/mbs/src/evaluator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/mbs/src/script.cpp
)

target_include_directories(mbs
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/json/single_include
)

target_include_directories(mantisbase
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/mbs/include
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/mbs/src
)

target_link_libraries(mantisbase PUBLIC mbs)