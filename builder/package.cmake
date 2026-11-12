file(MAKE_DIRECTORY "${DIST}/fonts")
file(COPY "${SITE_SOURCE}/favicon.ico" "${SITE_SOURCE}/pages/page.css"
    "${CMAKE_CURRENT_LIST_DIR}/site.css" DESTINATION "${DIST}")

foreach(font IN ITEMS
    "random/fonts/IBM-VGA-8x16.woff"
    "random/fonts/VGA-LICENSE.txt"
    "random/fonts/VGA-README.txt"
    "random/fonts/Open Font License.txt"
    "random/fonts/din1451alt.ttf"
    "pages/fonts/NotoSerifSC-Regular.woff2"
    "pages/fonts/NotoSerifSC-Regular.woff"
    "pages/fonts/NotoSerifSC-OFL.txt"
    "pages/fonts/GOSTLettering-Regular.woff2"
    "pages/fonts/GOSTLettering-Regular.woff"
)
    file(COPY "${SITE_SOURCE}/${font}" DESTINATION "${DIST}/fonts")
endforeach()

foreach(page IN ITEMS about friends contact)
    file(MAKE_DIRECTORY "${DIST}/${page}")
    execute_process(
        COMMAND "${PACK_PAGE}" "${page}" "${SITE_SOURCE}/source/${page}.md"
            "${CMAKE_CURRENT_LIST_DIR}/templates/${page}.html" "${DIST}/${page}/index.html"
        COMMAND_ERROR_IS_FATAL ANY
    )
endforeach()

execute_process(
    COMMAND "${PACK_HOME}" "${HOME_STAGE}"
    WORKING_DIRECTORY "${SITE_SOURCE}"
    COMMAND_ERROR_IS_FATAL ANY
)
file(RENAME "${HOME_STAGE}" "${DIST}/index.html")
