# tests/CMakeLists.txt

# Phase 8: several tests call fopen()/remove() directly (test_phase7,
# test_writer's tmpfile() path indirectly too); apply the same
# MSVC CRT-deprecation-warning suppression used for the library itself
# to every test target below, directory-wide, rather than repeating it
# on each add_executable() call. Also apply the same warning-flag
# posture as the library for consistency.
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

add_executable(test_basic test_basic.c)
target_link_libraries(test_basic PRIVATE xconf)
add_test(NAME test_basic COMMAND test_basic)

# test_value_store links the value-store source directly, not the
# whole xconf library, so it builds even without flex/bison.
add_executable(test_value_store
    test_value_store.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/xconf_value.c
)
target_include_directories(test_value_store PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
add_test(NAME test_value_store COMMAND test_value_store)

add_executable(test_phase2 test_phase2.c)
target_link_libraries(test_phase2 PRIVATE xconf)
add_test(NAME test_phase2 COMMAND test_phase2)

add_executable(test_phase3 test_phase3.c)
target_link_libraries(test_phase3 PRIVATE xconf)
add_test(NAME test_phase3 COMMAND test_phase3)

add_executable(test_phase4 test_phase4.c)
target_link_libraries(test_phase4 PRIVATE xconf)
add_test(NAME test_phase4 COMMAND test_phase4)

# test_phase5 needs xconf_internal.h/xconf_value.h (the test-only
# xconf_internal_document() accessor and the node-list layout), so it
# gets access to src/ in addition to the public include/ directory.
add_executable(test_phase5 test_phase5.c)
target_link_libraries(test_phase5 PRIVATE xconf)
target_include_directories(test_phase5 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
add_test(NAME test_phase5 COMMAND test_phase5)

# test_phase6 also uses xconf_internal_document() to check node counts.
add_executable(test_phase6 test_phase6.c)
target_link_libraries(test_phase6 PRIVATE xconf)
target_include_directories(test_phase6 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
add_test(NAME test_phase6 COMMAND test_phase6)

# test_writer links xconf_value.c + xconf_writer.c directly, not the
# whole xconf library, so it builds even without flex/bison.
add_executable(test_writer
    test_writer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/xconf_value.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/xconf_writer.c
)
target_include_directories(test_writer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
add_test(NAME test_writer COMMAND test_writer)

# test_phase7 (parse -> save -> reload) also needs xconf_internal.h.
add_executable(test_phase7 test_phase7.c)
target_link_libraries(test_phase7 PRIVATE xconf)
target_include_directories(test_phase7 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
add_test(NAME test_phase7 COMMAND test_phase7)

# test_phase9 (hex/binary literals) also needs xconf_internal.h to
# inspect the origin-preserving trailing comments.
add_executable(test_phase9 test_phase9.c)
target_link_libraries(test_phase9 PRIVATE xconf)
target_include_directories(test_phase9 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
add_test(NAME test_phase9 COMMAND test_phase9)

# test_phase10 (xconf_foreach) only needs the public header.
add_executable(test_phase10 test_phase10.c)
target_link_libraries(test_phase10 PRIVATE xconf)
add_test(NAME test_phase10 COMMAND test_phase10)
