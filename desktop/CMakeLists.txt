# install the .desktop and rc files in the correct place

configure_file(win.amandoti.lorgn.desktop.cmake ${CMAKE_CURRENT_BINARY_DIR}/win.amandoti.lorgn.desktop)
install(
    PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/win.amandoti.lorgn.desktop
    DESTINATION ${KDE_INSTALL_APPDIR}
)

install( DIRECTORY DESTINATION "${KDE_INSTALL_FULL_DATAROOTDIR}/kglobalaccel" )
install(
    CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink \"${KDE_INSTALL_FULL_APPDIR}/win.amandoti.lorgn.desktop\" \"\$ENV{DESTDIR}${KDE_INSTALL_FULL_DATAROOTDIR}/kglobalaccel/win.amandoti.lorgn.desktop\")"
)

install(
    FILES lorgn.notifyrc
    DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR}
)

