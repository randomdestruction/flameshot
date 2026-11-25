// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Contributors

#pragma once

#include "tools/imgupload/storages/imguploaderbase.h"

#include <QUrl>
#include <QWidget>

class QNetworkReply;
class QNetworkAccessManager;
class QUrl;

class ImgTacoUploader : public ImgUploaderBase
{
    Q_OBJECT
public:
    explicit ImgTacoUploader(const QPixmap& capture, QWidget* parent = nullptr);
    void deleteImage(const QString& fileName, const QString& deleteToken);

private:
    void upload();
    void handleUploadReply(QNetworkReply* reply);
    void handleDeleteReply(QNetworkReply* reply);
    void notify(const QString& message, bool isError = false);
    void finishDelete();

private:
    QNetworkAccessManager* m_NetworkAM;
};
