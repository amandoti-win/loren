[Desktop Entry]
GenericName=Screenshot Capture and Upload Utility
Name=Lorgn
Comment=Screenshot capture utility
Categories=Qt;KDE;Utility;
Keywords=snapshot;capture;print;screenshot;snipping;snipping tool;snip;
Exec=${KDE_INSTALL_FULL_BINDIR}/lorgn
Icon=lorgn
Type=Application
StartupNotify=false
Actions=FullScreenScreenShot;CurrentMonitorScreenShot;ActiveWindowScreenShot;RectangularRegionScreenShot;WindowUnderCursorScreenShot;RecordRegion;RecordScreen;RecordWindow;OpenWithoutScreenshot;
DBusActivatable=true
X-DBUS-StartupType=Unique
X-DBUS-ServiceName=win.amandoti.Lorgn
X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2
X-KDE-Wayland-Interfaces=org_kde_plasma_window_management,zkde_screencast_unstable_v1

[Desktop Action FullScreenScreenShot]
Name=Capture Entire Desktop
Exec=${KDE_INSTALL_FULL_BINDIR}/lorgn -f

[Desktop Action CurrentMonitorScreenShot]
Name=Capture Current Monitor
Exec=${KDE_INSTALL_FULL_BINDIR}/lorgn -m


[Desktop Action ActiveWindowScreenShot]
Name=Capture Active Window
Exec=${KDE_INSTALL_FULL_BINDIR}/lorgn -a

[Desktop Action RectangularRegionScreenShot]
Name=Capture Rectangular Region
Exec=${KDE_INSTALL_FULL_BINDIR}/lorgn -r

[Desktop Action WindowUnderCursorScreenShot]
Name=Capture Window Under Cursor
Exec=${KDE_INSTALL_FULL_BINDIR}/lorgn -u

[Desktop Action RecordRegion]
Name=Record Rectangular Region
Exec=${KDE_INSTALL_FULL_BINDIR}/lorgn -R region

[Desktop Action RecordScreen]
Name=Record Screen
Exec=${KDE_INSTALL_FULL_BINDIR}/lorgn -R screen

[Desktop Action RecordWindow]
Name=Record Window
Exec=${KDE_INSTALL_FULL_BINDIR}/lorgn -R window

[Desktop Action OpenWithoutScreenshot]
Name=Launch without taking a screenshot
Exec=${KDE_INSTALL_FULL_BINDIR}/lorgn -l
