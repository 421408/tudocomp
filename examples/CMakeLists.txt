# Create executable for every *.cpp file
add_custom_target(examples)

FILE(GLOB children RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)
FOREACH(child ${children})
    IF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${child})
        add_subdirectory(${child})
    ENDIF()
ENDFOREACH()

add_executable(
    coder_stats_example

    coder_stats.cpp
        ../include/tudocomp/compressors/lz77Aprox/berenstein_hash.hpp ../include/tudocomp/compressors/lz77Aprox/dna_nth_hash.hpp ../include/tudocomp/compressors/lz77Aprox/buz_hash.hpp ../include/tudocomp/compressors/LZ77AproxCompressor-nthash.hpp ../include/tudocomp/compressors/LZ77AproxCompressor-rabin-karp.hpp ../include/tudocomp/compressors/LZ77AproxCompressor-buz.hpp ../include/tudocomp/compressors/LZ77AproxCompressor-djb2.hpp ../include/tudocomp/compressors/lz77Aprox/hash_interface_32.hpp)

target_link_libraries(
    coder_stats_example

    ${TDC_DEPENDS}
    tudocomp
    bit_span
)

add_dependencies(examples coder_stats_example)

