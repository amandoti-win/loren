/* Loren, based on KDE Spectacle
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "UploadOptionsPage.h"

#include "Uploader.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QStandardPaths>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace
{
const QString AuthHeader = u"Authorization"_s;
const QString CfIdHeader = u"CF-Access-Client-Id"_s;
const QString CfSecretHeader = u"CF-Access-Client-Secret"_s;

QString ownConfigPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/loren/upload.json"_s;
}

QLineEdit *secretEdit(QWidget *parent)
{
    auto *edit = new QLineEdit(parent);
    edit->setEchoMode(QLineEdit::Password);
    return edit;
}
}

UploadOptionsPage::UploadOptionsPage(QWidget *parent)
    : QWidget(parent)
    , m_copyUploads(new QCheckBox(i18n("Copy button uploads and copies the link"), this))
    , m_url(new QLineEdit(this))
    , m_method(new QComboBox(this))
    , m_body(new QComboBox(this))
    , m_fileField(new QLineEdit(this))
    , m_authorization(secretEdit(this))
    , m_cfId(new QLineEdit(this))
    , m_cfSecret(secretEdit(this))
    , m_extraHeaders(new QPlainTextEdit(this))
    , m_responseKind(new QComboBox(this))
    , m_responseValue(new QLineEdit(this))
    , m_link(new QLineEdit(this))
    , m_pathLabel(new QLabel(this))
    , m_testResult(new QLabel(this))
    , m_testButton(new QPushButton(i18n("Save and test upload"), this))
{
    // KConfigDialog binds this to the Settings entry by its name.
    m_copyUploads->setObjectName(u"kcfg_copyUploadsLink"_s);
    m_copyUploads->setToolTip(i18n("When you click Copy, upload the image to your server and copy the link instead of the image."));

    m_method->addItems({u"PUT"_s, u"POST"_s, u"PATCH"_s});
    m_body->addItem(i18n("Raw image"), u"raw"_s);
    m_body->addItem(i18n("Multipart form"), u"multipart"_s);
    m_responseKind->addItem(i18n("JSON pointer"), u"json_pointer"_s);
    m_responseKind->addItem(i18n("Regular expression"), u"regex"_s);
    m_responseKind->addItem(i18n("Plain text"), u"text"_s);

    m_url->setPlaceholderText(u"https://files.example.com/api/upload"_s);
    m_fileField->setPlaceholderText(u"file"_s);
    m_authorization->setPlaceholderText(i18n("e.g. Bearer <token>"));
    m_cfId->setPlaceholderText(i18n("Cloudflare Access service token client ID"));
    m_cfSecret->setPlaceholderText(i18n("Cloudflare Access service token client secret"));
    m_extraHeaders->setPlaceholderText(u"Header-Name: value"_s);
    m_extraHeaders->setMaximumHeight(60);
    m_responseValue->setPlaceholderText(u"/key"_s);
    m_link->setPlaceholderText(u"https://files.example.com/f/{value}"_s);
    m_link->setToolTip(i18n("Use {value} for what was read from the response, {filename} for the file name."));
    m_expires = new QComboBox(this);
    m_expires->addItem(i18n("Never"), 0);
    m_expires->addItem(i18n("1 hour"), 3600);
    m_expires->addItem(i18n("1 day"), 86400);
    m_expires->addItem(i18n("7 days"), 604800);
    m_expires->addItem(i18n("30 days"), 2592000);
    m_expires->setToolTip(i18n("Sent to the server as expires=<seconds>. Your server has to support it."));
    m_deletePointer = new QLineEdit(this);
    m_deletePointer->setPlaceholderText(u"/delete_token"_s);
    m_deletePointer->setToolTip(i18n("Where the server's reply holds the secret delete token. Leave empty if the server needs no token, for example when it is behind a login."));
    m_deleteLink = new QLineEdit(this);
    m_deleteLink->setPlaceholderText(u"https://files.example.com/f/{value}?token={delete}"_s);
    m_deleteLink->setToolTip(i18n("Address that deletes the upload. {value} is the link value and {delete} the delete token, if you set one. Leave empty for no Delete button."));
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pathLabel->setWordWrap(true);
    m_testResult->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_testResult->setWordWrap(true);

    auto *form = new QFormLayout;
    form->addRow(i18n("Server URL:"), m_url);
    form->addRow(i18n("Method:"), m_method);
    form->addRow(i18n("Send image as:"), m_body);
    form->addRow(i18n("Form field name:"), m_fileField);
    form->addRow(i18n("Authorization header:"), m_authorization);
    form->addRow(i18n("Access client ID:"), m_cfId);
    form->addRow(i18n("Access client secret:"), m_cfSecret);
    form->addRow(i18n("Other headers:"), m_extraHeaders);
    form->addRow(i18n("Read link from response as:"), m_responseKind);
    form->addRow(i18n("Response value:"), m_responseValue);
    form->addRow(i18n("Link template:"), m_link);
    form->addRow(i18n("Links expire after:"), m_expires);
    form->addRow(i18n("Delete token from response:"), m_deletePointer);
    form->addRow(i18n("Delete link:"), m_deleteLink);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_copyUploads);
    layout->addLayout(form);
    layout->addWidget(m_testButton, 0, Qt::AlignLeft);
    layout->addWidget(m_testResult);
    layout->addStretch();
    layout->addWidget(m_pathLabel);

    for (auto *edit : {m_url, m_fileField, m_authorization, m_cfId, m_cfSecret, m_responseValue, m_link, m_deletePointer, m_deleteLink}) {
        connect(edit, &QLineEdit::textEdited, this, &UploadOptionsPage::markChanged);
    }
    connect(m_extraHeaders, &QPlainTextEdit::textChanged, this, [this] {
        if (m_extraHeaders->hasFocus()) {
            markChanged();
        }
    });
    for (auto *combo : {m_method, m_body, m_responseKind, m_expires}) {
        connect(combo, &QComboBox::activated, this, &UploadOptionsPage::markChanged);
    }
    connect(m_testButton, &QPushButton::clicked, this, &UploadOptionsPage::testUpload);

    reload();
}

bool UploadOptionsPage::isModified() const
{
    return m_modified;
}

void UploadOptionsPage::markChanged()
{
    m_modified = true;
    Q_EMIT changed();
}

void UploadOptionsPage::reload()
{
    m_config = {};
    const QString path = Uploader::configPath();
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        m_config = QJsonDocument::fromJson(file.readAll()).object();
    }

    m_url->setText(m_config.value(u"url"_s).toString());
    m_method->setCurrentText(m_config.value(u"method"_s).toString(u"PUT"_s).toUpper());
    m_body->setCurrentIndex(qMax(0, m_body->findData(m_config.value(u"body"_s).toString(u"raw"_s))));
    m_fileField->setText(m_config.value(u"file_field"_s).toString());

    m_authorization->clear();
    m_cfId->clear();
    m_cfSecret->clear();
    QStringList extra;
    const QJsonObject headers = m_config.value(u"headers"_s).toObject();
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        const QString value = it.value().toString();
        if (it.key().compare(AuthHeader, Qt::CaseInsensitive) == 0) {
            m_authorization->setText(value);
        } else if (it.key().compare(CfIdHeader, Qt::CaseInsensitive) == 0) {
            m_cfId->setText(value);
        } else if (it.key().compare(CfSecretHeader, Qt::CaseInsensitive) == 0) {
            m_cfSecret->setText(value);
        } else {
            extra << it.key() + u": "_s + value;
        }
    }
    m_extraHeaders->setPlainText(extra.join(u'\n'));

    const QJsonObject response = m_config.value(u"response"_s).toObject();
    int kind = 0;
    QString value;
    for (int i = 0; i < m_responseKind->count(); ++i) {
        const QString key = m_responseKind->itemData(i).toString();
        if (response.contains(key)) {
            kind = i;
            value = response.value(key).isString() ? response.value(key).toString() : QString();
        }
    }
    m_responseKind->setCurrentIndex(kind);
    m_responseValue->setText(value);
    m_link->setText(m_config.value(u"link"_s).toString());
    const int expires = m_config.value(u"expires"_s).toInt(0);
    int expiresIndex = m_expires->findData(expires);
    if (expiresIndex < 0) {
        m_expires->addItem(i18n("%1 seconds", expires), expires);
        expiresIndex = m_expires->count() - 1;
    }
    m_expires->setCurrentIndex(expiresIndex);
    m_deletePointer->setText(m_config.value(u"delete_pointer"_s).toString());
    m_deleteLink->setText(m_config.value(u"delete_link"_s).toString());

    m_pathLabel->setText(i18n("Server settings are saved to %1 (readable only by you).", ownConfigPath())
                         + (path != ownConfigPath() && QFileInfo::exists(path) ? u"\n"_s + i18n("Currently reading %1; saving here moves it.", path) : QString()));
    m_modified = false;
}

void UploadOptionsPage::save()
{
    QJsonObject cfg = m_config;
    cfg.insert(u"url"_s, m_url->text().trimmed());
    cfg.insert(u"method"_s, m_method->currentText());
    cfg.insert(u"body"_s, m_body->currentData().toString());
    if (m_fileField->text().trimmed().isEmpty()) {
        cfg.remove(u"file_field"_s);
    } else {
        cfg.insert(u"file_field"_s, m_fileField->text().trimmed());
    }
    if (!cfg.contains(u"query"_s) && m_body->currentData().toString() == u"raw") {
        cfg.insert(u"query"_s, QJsonObject{{u"name"_s, u"{filename}"_s}});
    }

    QJsonObject headers;
    auto put = [&headers](const QString &name, const QString &value) {
        if (!value.trimmed().isEmpty()) {
            headers.insert(name, value.trimmed());
        }
    };
    put(AuthHeader, m_authorization->text());
    put(CfIdHeader, m_cfId->text());
    put(CfSecretHeader, m_cfSecret->text());
    const auto lines = m_extraHeaders->toPlainText().split(u'\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const int colon = line.indexOf(u':');
        if (colon > 0) {
            put(line.left(colon).trimmed(), line.mid(colon + 1));
        }
    }
    cfg.insert(u"headers"_s, headers);

    cfg.insert(u"response"_s, QJsonObject{{m_responseKind->currentData().toString(),
                                            m_responseKind->currentData().toString() == u"text" ? QJsonValue(true) : QJsonValue(m_responseValue->text())}});
    if (m_link->text().trimmed().isEmpty()) {
        cfg.remove(u"link"_s);
    } else {
        cfg.insert(u"link"_s, m_link->text().trimmed());
    }

    const int expires = m_expires->currentData().toInt();
    if (expires > 0) {
        cfg.insert(u"expires"_s, expires);
    } else {
        cfg.remove(u"expires"_s);
    }
    for (const auto &[key, edit] : {std::pair{u"delete_pointer"_s, m_deletePointer}, std::pair{u"delete_link"_s, m_deleteLink}}) {
        if (edit->text().trimmed().isEmpty()) {
            cfg.remove(key);
        } else {
            cfg.insert(key, edit->text().trimmed());
        }
    }

    const QString path = ownConfigPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    // Restrict before writing so the secret is never readable by others, even briefly.
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        m_testResult->setText(i18n("Could not write %1", path));
        return;
    }
    file.write(QJsonDocument(cfg).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        m_testResult->setText(i18n("Could not write %1", path));
        return;
    }
    m_config = cfg;
    m_modified = false;
    m_pathLabel->setText(i18n("Server settings are saved to %1 (readable only by you).", path));
}

void UploadOptionsPage::testUpload()
{
    save();
    m_testButton->setEnabled(false);
    m_testResult->setText(i18n("Uploading a test image..."));

    QImage image(200, 80, QImage::Format_ARGB32);
    image.fill(QColor(30, 30, 30));
    {
        QPainter painter(&image);
        painter.setPen(Qt::white);
        painter.drawText(image.rect(), Qt::AlignCenter, u"Loren test"_s);
    }

    auto *uploader = new Uploader(this);
    connect(uploader, &Uploader::finished, this, [this, uploader](const QString &link) {
        m_testResult->setText(i18n("Worked. Link: %1", link));
        m_testButton->setEnabled(true);
        uploader->deleteLater();
    });
    connect(uploader, &Uploader::failed, this, [this, uploader](const QString &message) {
        m_testResult->setText(i18n("Failed: %1", message));
        m_testButton->setEnabled(true);
        uploader->deleteLater();
    });
    uploader->upload(image, u"loren-test.png"_s);
}

#include "moc_UploadOptionsPage.cpp"
