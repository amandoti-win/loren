/* Lorgn, based on KDE Spectacle
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QNetworkAccessManager;

/**
 * Uploads an image to the user's own server and reports the resulting link.
 *
 * The config file is JSON with the same keys as spectacle-uploader:
 * url, method, body, file_field, query, headers, response, link, timeout.
 */
class Uploader : public QObject
{
    Q_OBJECT
public:
    explicit Uploader(QObject *parent = nullptr);

    /// ~/.config/lorgn/upload.json, falling back to the spectacle-uploader config.
    static QString configPath();

    /// Starts the upload. Emits exactly one of finished() or failed().
    void upload(const QImage &image, const QString &filename);
    /// Same for a file on disk (a screen recording). The file is streamed, not loaded into memory.
    void uploadFile(const QString &path, const QString &filename, const QString &mime);
    /// Sends DELETE to a delete address returned by a previous upload. Emits deleted() or failed().
    void deleteRemote(const QString &url);

Q_SIGNALS:
    /// deleteUrl is empty when the server did not offer a way to delete the upload.
    void finished(const QString &link, const QString &deleteUrl);
    void deleted();
    void failed(const QString &message);

private:
    void start(const QByteArray &data, const QString &filePath, const QString &filename, const QString &mime);

    QNetworkAccessManager *m_nam;
};
