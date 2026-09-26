/*
 *  SPDX-FileCopyrightText: 2019 David Redondo <kde@david-redondo.de>
 *  SPDX-FileCopyrightText: 2015 Boudhayan Gupta <bgupta@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "Config.h"
#include "ShortcutActions.h"
#include "SpectacleCore.h"
#include "CommandLineOptions.h"
#include "SpectacleDBusAdapter.h"
#include "ScreenShotEffect.h"
#include "settings.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDir>
#include <QIcon>
#include <QSessionManager>

#include <KAboutData>
#include <KCrash>
#include <KDBusService>
#include <KLocalizedString>
#include <KMessageBox>
#include <KWindowSystem>

using namespace Qt::StringLiterals;

int main(int argc, char **argv)
{
    // set up the application

    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    QIcon::setFallbackThemeName(u"breeze"_s);
    QApplication app(argc, argv);

    // Lorgn's own accent is oxblood. It only replaces Plasma's stock blue highlight, so themes and
    // accent colours the user chose themselves are left alone.
    {
        QPalette palette = app.palette();
        const QColor stockBlue(0x3d, 0xae, 0xe9);
        if (palette.color(QPalette::Active, QPalette::Highlight) == stockBlue) {
            const bool dark = palette.color(QPalette::Window).lightness() < 128;
            const QColor accent = dark ? QColor(0xb8, 0x3a, 0x5a) : QColor(0x7a, 0x1a, 0x34);
            for (auto group : {QPalette::Active, QPalette::Inactive}) {
                palette.setColor(group, QPalette::Highlight, accent);
                palette.setColor(group, QPalette::Accent, accent);
                palette.setColor(group, QPalette::HighlightedText, Qt::white);
            }
            app.setPalette(palette);
        }
    }

    KLocalizedString::setApplicationDomain(QByteArrayLiteral("spectacle"));
    QCoreApplication::setOrganizationDomain(u"amandoti.win"_s);

    KAboutData aboutData(u"lorgn"_s,
                         u"Lorgn"_s,
                         QStringLiteral(SPECTACLE_VERSION),
                         i18n("Instant screenshot capture with shareable links on your own domain"),
                         KAboutLicense::GPL_V3,
                         u"(C) 2026 amandoti.win"_s);
    aboutData.setOtherText(u"Fork of Spectacle 6.3.5"_s);
    aboutData.setHomepage(u"https://github.com/amandoti-win/lorgn"_s);
    aboutData.addAuthor(u"amandoti.win"_s, u"Lorgn"_s, {}, u"https://github.com/amandoti-win/lorgn"_s);
    aboutData.addAuthor(u"Boudhayan Gupta"_s, u"Spectacle"_s, u"bgupta@kde.org"_s);
    aboutData.addAuthor(u"David Redondo"_s, u"Spectacle"_s, u"kde@david-redondo.de"_s);
    aboutData.addAuthor(u"Noah Davis"_s, u"Spectacle"_s, u"noahadvs@gmail.com"_s);
    aboutData.setCustomAuthorText(u"Report bugs at https://github.com/amandoti-win/lorgn/issues"_s,
                                  u"Report bugs at <a href=\"https://github.com/amandoti-win/lorgn/issues\">github.com/amandoti-win/lorgn/issues</a>"_s);
    aboutData.setTranslator(i18nc("NAME OF TRANSLATORS", "Your names"), i18nc("EMAIL OF TRANSLATORS", "Your emails"));
    aboutData.setOrganizationDomain("amandoti.win");
    aboutData.setDesktopFileName(u"win.amandoti.lorgn"_s);
    KAboutData::setApplicationData(aboutData);
    app.setWindowIcon(QIcon::fromTheme(u"lorgn"_s));

    KCrash::initialize();

    QCommandLineParser commandLineParser;
    aboutData.setupCommandLine(&commandLineParser);
    commandLineParser.addOptions(CommandLineOptions::self()->allOptions);

    // first parsing for help-about
    commandLineParser.process(app.arguments());
    aboutData.processCommandLine(&commandLineParser);

    // BUG: https://bugs.kde.org/show_bug.cgi?id=451842
    // We currently don't support desktop environments besides KDE Plasma on Wayland
    // because we have to rely on KWin's DBus API.
    if (KWindowSystem::isPlatformWayland() && !ScreenShotEffect::isLoaded()) {
        auto message = i18n("On Wayland, Lorgn requires the KWin compositor, which does not seem to be available. Use Lorgn with KWin, or use a different screenshot tool.");
        qWarning().noquote() << message;
        if (commandLineParser.isSet(CommandLineOptions::self()->background)
            || commandLineParser.isSet(CommandLineOptions::self()->dbus)) {
            // Return early if not in GUI mode.
            return 1;
        } else {
            KMessageBox::error(nullptr, message);
        }
    }

    // Prevent session manager from restoring the app on start up.
    // https://bugs.kde.org/show_bug.cgi?id=430411
    auto disableSessionManagement = [](QSessionManager &sm) {
        sm.setRestartHint(QSessionManager::RestartNever);
    };
    QObject::connect(&app, &QGuiApplication::commitDataRequest, disableSessionManagement);
    QObject::connect(&app, &QGuiApplication::saveStateRequest, disableSessionManagement);

    // If the new instance command line option has been specified,
    // use this alternative path for executing Spectacle.
    if (commandLineParser.isSet(CommandLineOptions::self()->newInstance)) {
        auto spectacleCore = SpectacleCore::instance();

        QObject::connect(qApp, &QApplication::aboutToQuit, Settings::self(), &Settings::save);
        QObject::connect(spectacleCore, &SpectacleCore::allDone, &app, &QCoreApplication::quit, Qt::QueuedConnection);

        // fire it up
        spectacleCore->activate(app.arguments(), QDir::currentPath());

        return app.exec();
    }

    // With the StartupOption::Unique flag, this process will exit during the construction of
    // KDBusService if Spectacle has already been registered.
    // This object does not need a parent since it will be deleted when it falls out of scope.
    KDBusService service(KDBusService::Unique);

    auto spectacleCore = SpectacleCore::instance();

    QObject::connect(&service, &KDBusService::activateRequested, spectacleCore, &SpectacleCore::activate);
    QObject::connect(&service, &KDBusService::activateActionRequested, spectacleCore, &SpectacleCore::activateAction);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, Settings::self(), &Settings::save);
    QObject::connect(spectacleCore, &SpectacleCore::allDone, &app, &QCoreApplication::quit, Qt::QueuedConnection);

    // create the dbus connections
    SpectacleDBusAdapter *dbusAdapter = new SpectacleDBusAdapter(spectacleCore);
    QObject::connect(spectacleCore, &SpectacleCore::dbusScreenshotFailed, dbusAdapter, &SpectacleDBusAdapter::ScreenshotFailed);
    QObject::connect(spectacleCore, &SpectacleCore::dbusRecordingFailed, dbusAdapter, &SpectacleDBusAdapter::RecordingFailed);
    QObject::connect(ExportManager::instance(),
                     &ExportManager::imageExported,
                     spectacleCore,
                     [dbusAdapter](const ExportManager::Actions &actions, const QUrl &url) {
                         Q_UNUSED(actions)
                         Q_EMIT dbusAdapter->ScreenshotTaken(url.toLocalFile());
                     });
    QObject::connect(ExportManager::instance(),
                     &ExportManager::videoExported,
                     spectacleCore,
                     [dbusAdapter](const ExportManager::Actions &actions, const QUrl &url) {
                         Q_UNUSED(actions)
                         Q_EMIT dbusAdapter->RecordingTaken(url.toLocalFile());
                     });
    QDBusConnection::sessionBus().registerObject(u"/"_s, spectacleCore);
    QDBusConnection::sessionBus().registerService(u"win.amandoti.Lorgn"_s);

    // fire it up
    spectacleCore->activate(app.arguments(), QDir::currentPath());

    return app.exec();
}
