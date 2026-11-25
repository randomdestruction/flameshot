// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Contributors

#include "imgtacouploader.h"
#include "utils/abstractlogger.h"
#include "utils/confighandler.h"
#include "utils/filenamehandler.h"
#include "utils/history.h"
#include "widgets/loadspinner.h"
#include "widgets/notificationwidget.h"

#include <QBuffer>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QShortcut>

namespace {

// json is empty when the server did not answer with JSON
QString failureText(QNetworkReply* reply, const QJsonObject& json)
{
    QString text = json[QStringLiteral("error")].toString();
    if (text.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
            text = reply->errorString();
        }
        return text;
    }
    const QString details = json[QStringLiteral("details")].toString();
    if (!details.isEmpty()) {
        text += "\n" + details;
    }
    return text;
}

// History entries are named "imgtaco-<image id>-<stored name>". The stored
// name can contain dashes, so the id field must never be left out.
constexpr const char* noImageId = "noid";

bool isImageId(const QString& text)
{
    if (text.isEmpty()) {
        return false;
    }
    for (const QChar c : text) {
        if (c < QLatin1Char('0') || c > QLatin1Char('9')) {
            return false;
        }
    }
    return true;
}

} // namespace

ImgTacoUploader::ImgTacoUploader(const QPixmap& capture, QWidget* parent)
  : ImgUploaderBase(capture, parent)
{
    m_NetworkAM = new QNetworkAccessManager(this);
}

void ImgTacoUploader::handleUploadReply(QNetworkReply* reply)
{
    reply->deleteLater();
    spinner()->deleteLater();
    m_currentImageName.clear();

    const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
    const QUrl url(json[QStringLiteral("url")].toString());

    // "filename" is only a display name
    QString storedName = json[QStringLiteral("stored_name")].toString();
    if (storedName.isEmpty()) {
        storedName = url.fileName();
    }

    const bool uploaded = reply->error() == QNetworkReply::NoError &&
                          json[QStringLiteral("success")].toBool() &&
                          url.isValid() && !storedName.isEmpty();

    if (uploaded) {
        setImageURL(url);

        // upload only tokens get no delete_url
        const QString deleteUrl = json[QStringLiteral("delete_url")].toString();
        if (deleteUrl.isEmpty()) {
            setDeleteUnavailable(
              tr("This image was uploaded with an upload only ImgTaco API "
                 "token, which cannot delete images."));
        }

        const QJsonValue id = json[QStringLiteral("id")];
        QString imageId =
          id.isDouble() ? QString::number(id.toInteger()) : id.toString();
        if (!isImageId(imageId)) {
            imageId = QUrl(deleteUrl).fileName();
        }
        if (!isImageId(imageId)) {
            imageId = noImageId;
        }

        History history;
        m_currentImageName =
          history.packFileName("imgtaco", imageId, storedName);
        history.save(pixmap(), m_currentImageName);

        emit uploadOk(imageURL());
    } else {
        const QString reason = failureText(reply, json);
        setInfoLabelText(reason.isEmpty() ? tr("Upload failed") : reason);
    }

    new QShortcut(Qt::Key_Escape, this, SLOT(close()));
}

void ImgTacoUploader::upload()
{
    QString apiToken = ConfigHandler().imgTacoApiToken();
    if (apiToken.isEmpty()) {
        setInfoLabelText(
          tr("ImgTaco API token not configured. Please set it in settings."));
        spinner()->deleteLater();
        new QShortcut(Qt::Key_Escape, this, SLOT(close()));
        return;
    }

    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    pixmap().save(&buffer, "PNG");

    QHttpMultiPart* multiPart =
      new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QString filename = FileNameHandler().parsedPattern() + ".png";
    QHttpPart imagePart;
    imagePart.setHeader(QNetworkRequest::ContentTypeHeader,
                        QVariant("image/png"));
    imagePart.setHeader(
      QNetworkRequest::ContentDispositionHeader,
      QVariant(
        QString("form-data; name=\"file\"; filename=\"%1\"").arg(filename)));
    imagePart.setBody(byteArray);
    multiPart->append(imagePart);

    // display name on the server
    QHttpPart filenamePart;
    filenamePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QVariant("form-data; name=\"filename\""));
    filenamePart.setBody(filename.toUtf8());
    multiPart->append(filenamePart);

    QUrl url(QStringLiteral("https://imgta.co/api/upload"));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization",
                         QStringLiteral("Bearer %1").arg(apiToken).toUtf8());

    QNetworkReply* reply = m_NetworkAM->post(request, multiPart);
    multiPart->setParent(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleUploadReply(reply);
    });
}

void ImgTacoUploader::deleteImage(const QString& fileName,
                                  const QString& deleteToken)
{
    Q_UNUSED(fileName)

    QString apiToken = ConfigHandler().imgTacoApiToken();
    if (apiToken.isEmpty()) {
        notify(tr("ImgTaco API token not configured."), true);
        finishDelete();
        return;
    }

    // deleteToken is the image id, the API accepts nothing else
    if (!isImageId(deleteToken)) {
        notify(tr("No ImgTaco image id is stored for this upload, so it "
                  "cannot be deleted from here."),
               true);
        finishDelete();
        return;
    }

    QUrl url(QStringLiteral("https://imgta.co/api/images/%1").arg(deleteToken));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization",
                         QStringLiteral("Bearer %1").arg(apiToken).toUtf8());

    QNetworkReply* reply = m_NetworkAM->deleteResource(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleDeleteReply(reply);
    });
}

void ImgTacoUploader::handleDeleteReply(QNetworkReply* reply)
{
    reply->deleteLater();

    const int httpStatus =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();

    const bool rejectedInBody = json.contains(QStringLiteral("success")) &&
                                !json[QStringLiteral("success")].toBool();

    if (reply->error() == QNetworkReply::NoError && !rejectedInBody) {
        notify(tr("Image deleted from ImgTaco."));
        emit deleteOk();
    } else if (httpStatus == 404) {
        // nothing left to delete, so the history entry can go as well
        notify(tr("ImgTaco has no such image, it may already be deleted."));
        emit deleteOk();
    } else if (httpStatus == 403 && json[QStringLiteral("code")].toString() ==
                                      QLatin1String("TOKEN_SCOPE")) {
        notify(tr("The ImgTaco API token can only upload. Deleting needs a "
                  "full access token."),
               true);
    } else {
        const QString reason = failureText(reply, json);
        notify(
          reason.isEmpty()
            ? tr("Unable to delete the image from ImgTaco.")
            : tr("Unable to delete the image from ImgTaco: %1").arg(reason),
          true);
    }

    finishDelete();
}

void ImgTacoUploader::notify(const QString& message, bool isError)
{
    // there is no notification widget when deleting from the upload history
    if (notification() != nullptr) {
        notification()->showMessage(message);
    } else if (isError) {
        AbstractLogger::error() << message;
    } else {
        AbstractLogger::info() << message;
    }
}

void ImgTacoUploader::finishDelete()
{
    // the upload history creates uploaders that are never shown or owned
    if (!isVisible()) {
        deleteLater();
    }
}
