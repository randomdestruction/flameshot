// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Yurii Puchkov & Contributors
//

#include "imguploadermanager.h"
// TODO - remove this hard-code and create plugin manager in the future, you may
// include other storage headers here
#ifdef ENABLE_IMGUR
#include "tools/imgupload/storages/imgur/imguruploader.h"
#endif
#ifdef ENABLE_IMGTACO
#include "tools/imgupload/storages/imgtaco/imgtacouploader.h"
#endif
#include "utils/confighandler.h"

#include <QPixmap>
#include <QWidget>

namespace {
constexpr const char* imgurUrl = "https://imgur.com/";
constexpr const char* imgTacoUrl = "https://imgta.co/i/";
} // namespace

ImgUploaderManager::ImgUploaderManager(QObject* parent)
  : QObject(parent)
  , m_imgUploaderBase(nullptr)
{
    m_imgUploaderPlugin = ConfigHandler().uploadService();
    if (m_imgUploaderPlugin.isEmpty()) {
        m_imgUploaderPlugin = IMG_UPLOADER_STORAGE_DEFAULT;
    }
    init();
}

void ImgUploaderManager::init()
{
    if (m_imgUploaderPlugin.compare("imgtaco") == 0) {
        m_urlString = imgTacoUrl;
    } else {
        m_urlString = imgurUrl;
        m_imgUploaderPlugin = "imgur";
    }
}

ImgUploaderBase* ImgUploaderManager::uploader(const QPixmap& capture,
                                              QWidget* parent)
{
#if defined(ENABLE_IMGUR) && defined(ENABLE_IMGTACO)
    if (m_imgUploaderPlugin.compare("imgtaco") == 0) {
        m_imgUploaderBase =
          (ImgUploaderBase*)(new ImgTacoUploader(capture, parent));
    } else {
        m_imgUploaderBase =
          (ImgUploaderBase*)(new ImgurUploader(capture, parent));
    }
#elif defined(ENABLE_IMGTACO)
    m_imgUploaderBase =
      (ImgUploaderBase*)(new ImgTacoUploader(capture, parent));
#else
    m_imgUploaderBase = (ImgUploaderBase*)(new ImgurUploader(capture, parent));
#endif
    if (m_imgUploaderBase && !capture.isNull()) {
        m_imgUploaderBase->upload();
    }
    return m_imgUploaderBase;
}

ImgUploaderBase* ImgUploaderManager::uploader(const QString& imgUploaderPlugin)
{
    m_imgUploaderPlugin = imgUploaderPlugin;
    init();
    return uploader(QPixmap());
}

const QString& ImgUploaderManager::uploaderPlugin()
{
    return m_imgUploaderPlugin;
}

const QString& ImgUploaderManager::url()
{
    return m_urlString;
}

QString ImgUploaderManager::url(const QString& imgUploaderPlugin)
{
    if (imgUploaderPlugin.compare("imgtaco") == 0) {
        return imgTacoUrl;
    }
    if (imgUploaderPlugin.compare("imgur") == 0) {
        return imgurUrl;
    }
    return m_urlString;
}
