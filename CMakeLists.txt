cmake_minimum_required(VERSION 3.15)

set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@3")
set(OPENSSL_USE_STATIC_LIBS TRUE)

project(AlphaEngine VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(CURL REQUIRED)
set(IXWEBSOCKET_LIB /opt/homebrew/lib/libixwebsocket.a)



include_directories(
        ${PROJECT_SOURCE_DIR}/include
        ${OPENSSL_INCLUDE_DIR}
        /opt/homebrew/include
)

set(ALPHA_SOURCES
        src/alpha/alpha_engine.cpp
        src/alpha/indicators.cpp
        src/alpha/microstructure.cpp
        src/alpha/orderflow.cpp
        src/alpha/regime.cpp
        src/alpha/vwap.cpp
)

add_library(alpha_lib STATIC ${ALPHA_SOURCES})

target_link_libraries(alpha_lib
        Threads::Threads
)

set(FEEDS_SOURCES
        src/feeds/binance_feed.cpp
        src/feeds/coinbase_feed.cpp
        src/feeds/polygon_feed.cpp
        src/feeds/candle_aggregator.cpp
)

add_library(feeds_lib STATIC ${FEEDS_SOURCES})

target_link_libraries(feeds_lib
        alpha_lib
        ${IXWEBSOCKET_LIB}
        OpenSSL::SSL
        OpenSSL::Crypto
        ZLIB::ZLIB
        nlohmann_json::nlohmann_json
        Threads::Threads
        CURL::libcurl
)

set(BACKTEST_SOURCES
        src/backtest/backtester.cpp
        src/backtest/pnl.cpp
        src/backtest/sharpe.cpp
)

add_library(backtest_lib STATIC ${BACKTEST_SOURCES})

target_link_libraries(backtest_lib
        alpha_lib
        Threads::Threads
)

set(STORAGE_SOURCES
        src/storage/influx_writer.cpp
)

add_library(storage_lib STATIC ${STORAGE_SOURCES})

add_executable(alpha_engine src/main.cpp)

target_link_libraries(alpha_engine
        alpha_lib
        feeds_lib
        backtest_lib
        storage_lib
        ${IXWEBSOCKET_LIB}
        OpenSSL::SSL
        OpenSSL::Crypto
        ZLIB::ZLIB
        nlohmann_json::nlohmann_json
        Threads::Threads
        CURL::libcurl
)

if(APPLE)
    target_link_libraries(alpha_engine
            "-framework Security"
            "-framework CoreFoundation"
    )
endif()

install(TARGETS alpha_engine
        RUNTIME DESTINATION bin
)

install(DIRECTORY include/
        DESTINATION include/alpha_engine
        FILES_MATCHING PATTERN "*.h"
)

message(STATUS "")
message(STATUS "╔═══════════════════════════════════════════════════════╗")
message(STATUS "║   Alpha Engine v${PROJECT_VERSION} - Build Configuration       ║")
message(STATUS "╠═══════════════════════════════════════════════════════╣")
message(STATUS "║ Build Type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "║ C++ Standard:    C++${CMAKE_CXX_STANDARD}")
message(STATUS "║ Compiler:        ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "║ Build Tests:     DISABLED")
message(STATUS "╠═══════════════════════════════════════════════════════╣")
message(STATUS "║ Features:")
message(STATUS "║  ✅ VPIN (Flow Toxicity)")
message(STATUS "║  ✅ Hasbrouck Price Impact")
message(STATUS "║  ✅ Order Flow Imbalance")
message(STATUS "║  ✅ Regime Detection")
message(STATUS "║  ✅ Polygon REST Stock Feeds")
message(STATUS "║  ✅ Backtesting Framework")
message(STATUS "╚═══════════════════════════════════════════════════════╝")
message(STATUS "")
