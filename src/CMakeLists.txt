set(SRC_LIST
    main.cpp
    MySQLConnectionPool.cpp
    Connection.cpp)

add_executable(a.out ${SRC_LIST})

target_include_directories(a.out PUBLIC ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(a.out pthread mysqlcppconn)           