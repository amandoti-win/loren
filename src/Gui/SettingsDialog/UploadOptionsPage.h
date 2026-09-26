/* Lorgn, based on KDE Spectacle
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QJsonObject>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

/**
 * Settings page for uploading to your own server.
 * The server settings live in upload.json (mode 600), not in the normal config,
 * so tokens stay in a private file.
 */
class UploadOptionsPage : public QWidget
{
    Q_OBJECT

public:
    explicit UploadOptionsPage(QWidget *parent);

    bool isModified() const;
    void save();
    void reload();

Q_SIGNALS:
    void changed();

private:
    void markChanged();
    void testUpload();

    QJsonObject m_config; // full file contents, so keys this page doesn't show survive a save
    bool m_modified = false;

    QCheckBox *m_copyUploads;
    QLineEdit *m_url;
    QComboBox *m_method;
    QComboBox *m_body;
    QLineEdit *m_fileField;
    QLineEdit *m_authorization;
    QLineEdit *m_cfId;
    QLineEdit *m_cfSecret;
    QPlainTextEdit *m_extraHeaders;
    QComboBox *m_responseKind;
    QLineEdit *m_responseValue;
    QLineEdit *m_link;
    QComboBox *m_expires = nullptr;
    QLineEdit *m_deletePointer = nullptr;
    QLineEdit *m_deleteLink = nullptr;
    QLabel *m_pathLabel;
    QLabel *m_testResult;
    QPushButton *m_testButton;
};
