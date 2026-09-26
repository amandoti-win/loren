/* Lorgn, based on KDE Spectacle
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "Uploader.h"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

namespace
{
constexpr qint64 MaxResponseBytes = 1024 * 1024;

QString expand(const QString &tmpl, const QHash<QString, QString> &vars)
{
    static const QRegularExpression re(u"\\{(\\w+)\\}"_s);
    QString out;
    qsizetype last = 0;
    for (auto it = re.globalMatch(tmpl); it.hasNext();) {
        const auto m = it.next();
        out += tmpl.mid(last, m.capturedStart() - last);
        out += vars.value(m.captured(1), m.captured(0));
        last = m.capturedEnd();
    }
    return out + tmpl.mid(last);
}

QString configDir(const QString &app)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u'/' + app;
}

// Returns the link value or an empty string, setting error when the config is unusable.
QString extractValue(const QJsonObject &spec, const QString &text)
{
    if (spec.contains(u"json_pointer"_s)) {
        QJsonParseError parseError;
        const auto doc = QJsonDocument::fromJson(text.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            return {};
        }
        QJsonValue value = doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array());
        const auto parts = spec.value(u"json_pointer"_s).toString().split(u'/').mid(1);
        for (QString part : parts) {
            part.replace(u"~1"_s, u"/"_s).replace(u"~0"_s, u"~"_s);
            if (value.isArray()) {
                bool ok = false;
                const int index = part.toInt(&ok);
                value = ok ? value.toArray().at(index) : QJsonValue();
            } else if (value.isObject()) {
                value = value.toObject().value(part);
            } else {
                return {};
            }
        }
        return value.isString() ? value.toString() : (value.isDouble() ? QString::number(value.toDouble(), 'g', 15) : QString());
    }
    if (spec.contains(u"regex"_s)) {
        const auto m = QRegularExpression(spec.value(u"regex"_s).toString()).match(text);
        if (!m.hasMatch()) {
            return {};
        }
        return m.lastCapturedIndex() > 0 ? m.captured(1) : m.captured(0);
    }
    return text.trimmed();
}
}

Uploader::Uploader(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    m_nam->setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
}

QString Uploader::configPath()
{
    const QString own = configDir(u"lorgn"_s) + u"/upload.json"_s;
    if (QFileInfo::exists(own)) {
        return own;
    }
    const QString legacy = configDir(u"spectacle-uploader"_s) + u"/config.json"_s;
    return QFileInfo::exists(legacy) ? legacy : own;
}

void Uploader::upload(const QImage &image, const QString &filename)
{
    // Screenshots carry metadata such as the captured window's title and screen position.
    // Rebuild the image from its pixels only, so none of that is published.
    const QImage clean = QImage(image.constBits(), image.width(), image.height(), image.bytesPerLine(), image.format()).copy();
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    clean.save(&buffer, "PNG");
    buffer.close();
    start(png, QString(), filename, u"image/png"_s);
}

void Uploader::uploadFile(const QString &path, const QString &filename, const QString &mime)
{
    start(QByteArray(), path, filename, mime);
}

void Uploader::start(const QByteArray &data, const QString &filePath, const QString &filename, const QString &mime)
{
    auto fail = [this](const QString &message) {
        QMetaObject::invokeMethod(this, [this, message] { Q_EMIT failed(message); }, Qt::QueuedConnection);
    };

    const QString path = configPath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        fail(u"No upload config found at %1. Copy your spectacle-uploader config there."_s.arg(path));
        return;
    }
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        fail(u"Could not read %1: not a JSON object."_s.arg(path));
        return;
    }
    const QJsonObject cfg = doc.object();

    const QUrl baseUrl(cfg.value(u"url"_s).toString());
    if (!baseUrl.isValid() || (baseUrl.scheme() != u"http" && baseUrl.scheme() != u"https")) {
        fail(u"Config 'url' must be an http(s) URL."_s);
        return;
    }
    const QByteArray method = cfg.value(u"method"_s).toString(u"POST"_s).toUpper().toLatin1();
    if (method != "POST" && method != "PUT" && method != "PATCH") {
        fail(u"Config 'method' must be POST, PUT or PATCH."_s);
        return;
    }
    const QString bodyType = cfg.value(u"body"_s).toString(u"multipart"_s);
    if (bodyType != u"raw" && bodyType != u"multipart") {
        fail(u"Config 'body' must be 'raw' or 'multipart'."_s);
        return;
    }
    const QJsonObject response = cfg.value(u"response"_s).toObject(QJsonObject{{u"text"_s, true}});
    int specs = 0;
    for (const auto &key : {u"json_pointer"_s, u"regex"_s, u"text"_s}) {
        specs += response.contains(key);
    }
    if (specs != 1) {
        fail(u"Config 'response' needs exactly one of json_pointer, regex or text."_s);
        return;
    }

    QFile *body = nullptr;
    if (!filePath.isEmpty()) {
        body = new QFile(filePath);
        if (!body->open(QIODevice::ReadOnly)) {
            delete body;
            fail(u"Could not read %1"_s.arg(filePath));
            return;
        }
    }

    const QHash<QString, QString> vars{{u"filename"_s, filename}, {u"mime"_s, mime}};

    QUrl url = baseUrl;
    const QJsonObject queryCfg = cfg.value(u"query"_s).toObject();
    if (!queryCfg.isEmpty()) {
        QUrlQuery query(url);
        for (auto it = queryCfg.begin(); it != queryCfg.end(); ++it) {
            query.addQueryItem(it.key(), expand(it.value().toString(), vars));
        }
        url.setQuery(query);
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(int(cfg.value(u"timeout"_s).toInt(300) * 1000));
    // Cloudflare-fronted servers often block empty or library-default agents.
    request.setHeader(QNetworkRequest::UserAgentHeader, u"lorgn/1.0"_s);
    const QJsonObject headers = cfg.value(u"headers"_s).toObject();
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        request.setRawHeader(it.key().toUtf8(), expand(it.value().toString(), vars).toUtf8());
    }

    QNetworkReply *reply = nullptr;
    if (bodyType == u"raw") {
        if (!request.hasRawHeader("Content-Type")) {
            request.setHeader(QNetworkRequest::ContentTypeHeader, mime);
        }
        reply = body ? m_nam->sendCustomRequest(request, method, body) : m_nam->sendCustomRequest(request, method, data);
    } else {
        auto *multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentTypeHeader, mime);
        const QString field = cfg.value(u"file_field"_s).toString(u"file"_s);
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       u"form-data; name=\"%1\"; filename=\"%2\""_s.arg(field, filename));
        if (body) {
            part.setBodyDevice(body);
        } else {
            part.setBody(data);
        }
        multipart->append(part);
        reply = m_nam->sendCustomRequest(request, method, multipart);
        multipart->setParent(reply);
    }

    if (body) {
        body->setParent(reply);
    }

    const QString host = url.host();
    const QString linkTemplate = cfg.value(u"link"_s).toString(u"{value}"_s);
    connect(reply, &QNetworkReply::finished, this, [this, reply, host, response, linkTemplate, vars] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString text = QString::fromUtf8(reply->read(MaxResponseBytes));

        if (status >= 300 && status < 400) {
            Q_EMIT failed(u"%1 redirected the upload (HTTP %2); it probably wants a login, check your credentials."_s.arg(host).arg(status));
            return;
        }
        if (reply->error() != QNetworkReply::NoError && status == 0) {
            Q_EMIT failed(u"Could not reach %1: %2"_s.arg(host, reply->errorString()));
            return;
        }
        if (status >= 400) {
            const QString hint = (status == 401 || status == 403) ? u" - check your credentials"_s : QString();
            const QString detail = text.left(200).trimmed();
            Q_EMIT failed(u"%1 answered HTTP %2%3%4"_s.arg(host).arg(status).arg(hint, detail.isEmpty() ? QString() : u": "_s + detail));
            return;
        }

        const QString value = extractValue(response, text);
        if (value.isEmpty()) {
            Q_EMIT failed(u"Could not find the link in the server's response."_s);
            return;
        }
        auto allVars = vars;
        allVars.insert(u"value"_s, value);
        Q_EMIT finished(expand(linkTemplate, allVars));
    });
}
