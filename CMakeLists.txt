
cmake_minimum_required(VERSION 3.10)
project(openssl_digest_project C)

set(CMAKE_C_STANDARD 11)

find_package(OpenSSL REQUIRED)
include_directories(include)

add_executable(digest_app
    src/main.c
    src/queue.c
    src/cache.c
    src/digest.c
)

target_link_libraries(digest_app OpenSSL::Crypto pthread)
