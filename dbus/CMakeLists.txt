# install the DBus service and interface files in the correct place
include(ECMGenerateDBusServiceFile)
include(ECMConfiguredInstall)

# Generate and install dbus-activated systemd service
ecm_install_configured_files(INPUT app-win.amandoti.lorgn.service.in DESTINATION ${KDE_INSTALL_SYSTEMDUSERUNITDIR})

ecm_generate_dbus_service_file(
   NAME win.amandoti.lorgn
   EXECUTABLE "${KDE_INSTALL_FULL_BINDIR}/lorgn --dbus"
   SYSTEMD_SERVICE app-win.amandoti.lorgn.service
   DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR}
)
ecm_generate_dbus_service_file(
   NAME win.amandoti.Lorgn
   EXECUTABLE "${KDE_INSTALL_FULL_BINDIR}/lorgn --dbus"
   SYSTEMD_SERVICE app-win.amandoti.lorgn.service
   DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR}
)

install(FILES org.kde.Spectacle.xml DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR})
