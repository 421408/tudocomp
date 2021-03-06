if(NOT GTEST_FOUND)
    MESSAGE(STATUS "gtest is not available - tests disabled!")
    return()
endif()

include(tdc_testsuite)

list(APPEND BASIC_DEPS ${TDC_DEPENDS} ${GTEST_LIBRARIES} tudocomp_stat tudocomp)

# doc snippets
macro(run_doc_snippet snippet)
generic_run_test(
    "doc_${snippet}"
    "${CMAKE_CURRENT_SOURCE_DIR}/doc_snippets/${snippet}.cpp"
    "${TDC_TESTSUITE_DIR}/test_driver.cpp"
    ""
    check
    build_check
    "Test"
    ${ARGN}
)
endmacro()

run_doc_snippet(io                  DEPS ${BASIC_DEPS})
run_doc_snippet(bit_io              DEPS ${BASIC_DEPS})
run_doc_snippet(int_vector          DEPS ${BASIC_DEPS})
run_doc_snippet(bit_vector          DEPS ${BASIC_DEPS})
run_doc_snippet(algorithm_impl      DEPS ${BASIC_DEPS})
run_doc_snippet(coder_impl          DEPS ${BASIC_DEPS})
run_doc_snippet(compressor_impl     DEPS ${BASIC_DEPS})
run_doc_snippet(generator_impl      DEPS ${BASIC_DEPS})
run_doc_snippet(stats               DEPS ${BASIC_DEPS})

# other tests
run_test(rank_select_tests  DEPS ${BASIC_DEPS})
run_test(ringbuffer_tests   DEPS ${BASIC_DEPS})
run_test(bit_io_tests   DEPS ${BASIC_DEPS})
run_test(vbyte_test     DEPS ${BASIC_DEPS})
run_test(variadic_tests DEPS ${BASIC_DEPS})
run_test(rle_test       DEPS ${BASIC_DEPS})
run_test(mtf_test       DEPS ${BASIC_DEPS})
run_test(huff_test      DEPS ${BASIC_DEPS})
run_test(arithm_tests   DEPS ${BASIC_DEPS})
run_test(coder_tests    DEPS ${BASIC_DEPS})
run_test(cedar_tests    DEPS ${BASIC_DEPS})
run_test(st_tests       DEPS ${BASIC_DEPS})
run_test(maxlcp_tests   DEPS ${BASIC_DEPS})

run_test(lz_trie_tests  DEPS ${BASIC_DEPS})

run_test(lzss_test      DEPS ${BASIC_DEPS})

run_test(meta_tests     DEPS ${BASIC_DEPS})
run_test(tudocomp_tests DEPS ${BASIC_DEPS})
run_test(input_output_tests DEPS ${BASIC_DEPS})
run_test(ds_manager_tests   DEPS ${BASIC_DEPS})
run_test(ds_tests       DEPS ${BASIC_DEPS})
run_test(lcpsada_tests  DEPS ${BASIC_DEPS})
run_test(plcp_tests DEPS ${BASIC_DEPS})
run_test(esp_tests      DEPS ${BASIC_DEPS})

run_test(intsort_tests DEPS ${BASIC_DEPS})

set(SANDBOX_CPP ${CMAKE_CURRENT_SOURCE_DIR}/sandbox_tests.cpp)
if(NOT EXISTS ${SANDBOX_CPP})
    file(READ ${CMAKE_SOURCE_DIR}/etc/sandbox_tests.cpp.template SANDBOX_TEMPLATE)
    file(WRITE ${SANDBOX_CPP} "${SANDBOX_TEMPLATE}")
endif()

run_test(sandbox_tests  DEPS ${BASIC_DEPS})

run_test(tudocomp_driver_tests
    DEPS     tudocomp_algorithms ${GTEST_LIBRARIES}
    BIN_DEPS tudocomp_driver)
run_test(matrix_tests
    DEPS     tudocomp_algorithms ${GTEST_LIBRARIES}
    BIN_DEPS tudocomp_driver)
