/*********************************************************
 * Copyright (C) 2021, Val Doroshchuk <valbok@gmail.com> *
 *                                                       *
 * This file is part of QtAVPlayer.                      *
 * Free Qt Media Player based on FFmpeg.                 *
 *********************************************************/

#ifndef QAVAUDIOFILTER_P_H
#define QAVAUDIOFILTER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qavfilter_p.h"
#include "qavaudioinputfilter_p.h"
#include "qavaudiooutputfilter_p.h"
#include <QList>

QT_BEGIN_NAMESPACE

class QAVAudioFilterPrivate;
class Q_AVPLAYER_EXPORT QAVAudioFilter : public QAVFilter
{
public:
    QAVAudioFilter(const QString &name, const QList<QAVAudioInputFilter> &inputs, const QList<QAVAudioOutputFilter> &outputs, QObject *parent = nullptr);

    int write(const QAVFrame &frame) override;
    int read(QAVFrame &frame) override;

protected:
    Q_DECLARE_PRIVATE(QAVAudioFilter)
private:
    Q_DISABLE_COPY(QAVAudioFilter)
};

QT_END_NAMESPACE

#endif
