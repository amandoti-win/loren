# install the DBus service and interface files in the correct place
include(ECMGenerateDBusServiceFile)
include(ECMConfiguredInstall)

# Generate and install dbus-activated systemd service
ecm_install_configured_files(INPUT app-win.amandoti.loren.service.in DESTINATION ${KDE_INSTALL_SYSTEMDUSERUNITDIR})

ecm_generate_dbus_service_file(
   NAME win.amandoti.loren
   EXECUTABLE "${KDE_INSTALL_FULL_BINDIR}/loren --dbus"
   SYSTEMD_SERVICE app-win.amandoti.loren.service
   DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR}
)
ecm_generate_dbus_service_file(
   NAME win.amandoti.Loren
   EXECUTABLE "${KDE_INSTALL_FULL_BINDIR}/loren --dbus"
   SYSTEMD_SERVICE app-win.amandoti.loren.service
   DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR}
)

