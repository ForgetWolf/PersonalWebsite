if(NOT FONTFORGE_EXECUTABLE)
    unset(FONTFORGE_EXECUTABLE)
    find_program(FONTFORGE_EXECUTABLE NAMES fontforge.bat fontforge)
endif()
if(NOT FONTFORGE_EXECUTABLE)
    message(FATAL_ERROR "FontForge is required for gost-fonts. Set FONTFORGE_EXECUTABLE.")
endif()

file(MAKE_DIRECTORY "${OUT}/staging")
set(sfd "${OUT}/GOSTLettering-Regular.sfd")
set(woff "${OUT}/staging/GOSTLettering-Regular.woff")
set(woff2 "${OUT}/staging/GOSTLettering-Regular.woff2")
execute_process(COMMAND "${GENERATOR}" "${INPUT}" "${sfd}" COMMAND_ERROR_IS_FATAL ANY)
file(REMOVE "${woff}" "${woff2}")
execute_process(
    COMMAND "${FONTFORGE_EXECUTABLE}" -lang=ff -c
        "Open($1); Generate($2); Generate($3)" "${sfd}" "${woff}" "${woff2}"
    COMMAND_ERROR_IS_FATAL ANY
)
file(READ "${woff}" woff_header LIMIT 4)
file(READ "${woff2}" woff2_header LIMIT 4)
if(NOT woff_header STREQUAL "wOFF" OR NOT woff2_header STREQUAL "wOF2")
    message(FATAL_ERROR "FontForge did not produce both requested webfont formats.")
endif()
file(RENAME "${woff}" "${OUT}/GOSTLettering-Regular.woff")
file(RENAME "${woff2}" "${OUT}/GOSTLettering-Regular.woff2")
