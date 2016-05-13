# docs-main target - builds the main documentation
find_package(Pandoc)
if(PANDOC_FOUND)

    set(MAIN_LATEX_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/main/latex)
    set(MAIN_HTML_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/main/html)

    file(MAKE_DIRECTORY ${MAIN_LATEX_OUT_DIR})
    file(MAKE_DIRECTORY ${MAIN_HTML_OUT_DIR})

    add_custom_target(docs-main
        # convert markdown doc to html
        COMMAND ${PANDOC_BIN} -f markdown -t html -o ${MAIN_HTML_OUT_DIR}/_content.html ${CMAKE_CURRENT_SOURCE_DIR}/Documentation.md
        # copy html framework
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/html ${MAIN_HTML_OUT_DIR}

        # convert markdown doc to tex
        COMMAND ${PANDOC_BIN} -f markdown -t latex -o ${MAIN_LATEX_OUT_DIR}/_content.tex ${CMAKE_CURRENT_SOURCE_DIR}/Documentation.md
        # copy tex framework
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/latex ${MAIN_LATEX_OUT_DIR}
        # build pdf
        COMMAND pdflatex -synctex=1 -interaction=nonstopmode main.tex #pass 1
        COMMAND pdflatex -synctex=1 -interaction=nonstopmode main.tex #pass 2
        # move it to output directory
        COMMAND ${CMAKE_COMMAND} -E rename ${MAIN_LATEX_OUT_DIR}/main.pdf ${CMAKE_CURRENT_BINARY_DIR}/Documentation.pdf
        #
        WORKING_DIRECTORY ${MAIN_LATEX_OUT_DIR}
        COMMENT "Generating main documentation" VERBATIM
    )

endif(PANDOC_FOUND)

# docs-doxygen target - builds the Doxygen documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(docs-doxygen
        #invoke doxygen
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        # make doxygen PDF
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/doxygen/latex make
        # move it to output directory
        COMMAND ${CMAKE_COMMAND} -E rename ${CMAKE_CURRENT_BINARY_DIR}/doxygen/latex/refman.pdf ${CMAKE_CURRENT_BINARY_DIR}/Doxygen.pdf
        #
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating doxygen documentation"
    )
endif(DOXYGEN_FOUND)

# docs target - invokes docs-main and docs-doxygen
if(DOXYGEN_FOUND AND PANDOC_FOUND)
add_custom_target(docs DEPENDS docs-main docs-doxygen)
endif(DOXYGEN_FOUND AND PANDOC_FOUND)
