/****************************************************************************
**
** Copyright (C) 2023- Paolo Angelelli <paolo.angelelli@gmail.com>
**
** Commercial License Usage
** Licensees holding a valid commercial QmlPanorama license may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement with the copyright holder. For licensing terms
** and conditions and further information contact the copyright holder.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3. The licenses are as published by
** the Free Software Foundation at https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#ifndef QMLPANORAMA_H
#define QMLPANORAMA_H

#include "photosphere.h"

void registerQmlPanorama() {
    qmlRegisterType<QmlPhotoSphere>("QmlPanorama", 1, 0, "PhotoSphere");
}

#endif // QMLPANORAMA_H
