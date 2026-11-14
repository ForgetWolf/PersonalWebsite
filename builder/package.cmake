file(MAKE_DIRECTORY "${DIST}/fonts")
file(COPY "${SITE_SOURCE}/favicon.ico" "${SITE_SOURCE}/pages/page.css"
    "${SITE_SOURCE}/pages/coverflow.css" "${SITE_SOURCE}/menu.css"
    "${SITE_SOURCE}/welcome.css" "${SITE_SOURCE}/terminal.css" DESTINATION "${DIST}")
file(COPY "${SITE_SOURCE}/pages/favicon" DESTINATION "${DIST}"
    FILES_MATCHING PATTERN "*.png" PATTERN "*.svg" PATTERN "*.jpg"
    PATTERN "forgetwolf.png" EXCLUDE)

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
    "pages/fonts/fusion-pixel-12px-proportional-zh_hans.ttf.woff2"
    "pages/fonts/FusionPixel-OFL.txt"
    "pages/fonts/FusionPixel-LICENSES-ark-pixel-OFL.txt"
    "pages/fonts/FusionPixel-LICENSES-cubic-11-OFL.txt"
    "pages/fonts/FusionPixel-LICENSES-galmuri-LICENSE.txt"
)
    file(COPY "${SITE_SOURCE}/${font}" DESTINATION "${DIST}/fonts")
endforeach()

foreach(page IN ITEMS about friends contact)
    file(MAKE_DIRECTORY "${DIST}/${page}")
    unset(coverflow_template)
    if(page STREQUAL "friends")
        set(coverflow_template "${CMAKE_CURRENT_LIST_DIR}/templates/friends-coverflow.html")
    endif()
    execute_process(
        COMMAND "${PACK_PAGE}" "${page}" "${SITE_SOURCE}/source/${page}.md"
            "${CMAKE_CURRENT_LIST_DIR}/templates/${page}.html" "${DIST}/${page}/index.html"
            ${coverflow_template}
        COMMAND_ERROR_IS_FATAL ANY
    )
endforeach()

execute_process(
    COMMAND "${BUILD_HOME}"
    WORKING_DIRECTORY "${SITE_SOURCE}"
    COMMAND_ERROR_IS_FATAL ANY
)
