/*********************************************************
 * Copyright (C) 2020, Val Doroshchuk <valbok@gmail.com> *
 *                                                       *
 * This file is part of QtAVPlayer.                      *
 * Free Qt Media Player based on FFmpeg.                 *
 *********************************************************/

#ifndef QAVPACKETQUEUE_H
#define QAVPACKETQUEUE_H

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

#include "qavpacket_p.h"
#include "qavfilter_p.h"
#include "qavfiltergraph_p.h"
#include "qavframe.h"
#include "qavsubtitleframe.h"
#include "qavstreamframe.h"
#include <QMutex>
#include <QWaitCondition>
#include <QList>
#include <math.h>
#include <memory>

extern "C" {
#include <libavutil/time.h>
#include <libavcodec/avcodec.h>
}

QT_BEGIN_NAMESPACE

class QAVQueueClock
{
public:
    QAVQueueClock(double v = 1 / 24.0)
        : frameRate(v)
    {
    }

    bool wait(bool shouldSync, double pts, double speed = 1.0, double master = -1)
    {
        QMutexLocker locker(&m_mutex);
        double delay = pts - prevPts;
        if (isnan(delay) || delay <= 0 || delay > maxFrameDuration)
            delay = frameRate;

        if (master > 0) {
            double diff = pts - master;
            double sync_threshold = qMax(minThreshold, qMin(maxThreshold, delay));
            if (!isnan(diff) && fabs(diff) < maxFrameDuration) {
                if (diff <= -sync_threshold)
                    delay = qMax(0.0, delay + diff);
                else if (diff >= sync_threshold && delay > frameDuplicationThreshold)
                    delay = delay + diff;
                else if (diff >= sync_threshold)
                    delay = 2 * delay;
            }
        }

        delay /= speed;
        const double time = av_gettime_relative() / 1000000.0;
        if (shouldSync) {
            if (time < frameTimer + delay) {
                double remaining_time = qMin(frameTimer + delay - time, refreshRate);
                locker.unlock();
                av_usleep((int64_t)(remaining_time * 1000000.0));
                return false;
            }
        }

        prevPts = pts;
        frameTimer += delay;
        if ((delay > 0 && time - frameTimer > maxThreshold) || !shouldSync)
            frameTimer = time;

        return true;
    }

    double pts() const
    {
        QMutexLocker locker(&m_mutex);
        return prevPts;
    }

    void clear()
    {
        QMutexLocker locker(&m_mutex);
        prevPts = 0;
        frameTimer = 0;
    }

    void setFrameRate(double v)
    {
        QMutexLocker locker(&m_mutex);
        frameRate = v;
    }

private:
    double frameRate = 0;
    double frameTimer = 0;
    double prevPts = 0;
    mutable QMutex m_mutex;
    const double maxFrameDuration = 10.0;
    const double minThreshold = 0.04;
    const double maxThreshold = 0.1;
    const double frameDuplicationThreshold = 0.1;
    const double refreshRate = 0.01;
};

class QAVPacketQueue
{
public:
    QAVPacketQueue() = default;
    ~QAVPacketQueue()
    {
        abort();
    }

    bool isEmpty() const
    {
        QMutexLocker locker(&m_mutex);
        return m_packets.isEmpty();
    }

    void enqueue(const QAVPacket &packet)
    {
        QMutexLocker locker(&m_mutex);
        m_packets.append(packet);
        m_bytes += packet.packet()->size + sizeof(packet);
        m_duration += packet.duration();
        m_consumerWaiter.wakeAll();
        m_abort = false;
        m_waitingForPackets = false;
    }

    QAVPacket dequeue()
    {
        QMutexLocker locker(&m_mutex);
        if (m_packets.isEmpty()) {
            m_producerWaiter.wakeAll();
            if (!m_abort && !m_wake) {
                m_waitingForPackets = true;
                m_consumerWaiter.wait(&m_mutex);
                m_waitingForPackets = false;
            }
        }
        if (m_packets.isEmpty())
            return {};

        auto packet = m_packets.takeFirst();
        m_bytes -= packet.packet()->size + sizeof(packet);
        m_duration -= packet.duration();
        return packet;
    }

    void waitForEmpty()
    {
        QMutexLocker locker(&m_mutex);
        clearPackets();
        if (!m_abort && !m_waitingForPackets)
            m_producerWaiter.wait(&m_mutex);
    }

    void abort()
    {
        QMutexLocker locker(&m_mutex);
        m_abort = true;
        m_waitingForPackets = true;
        m_consumerWaiter.wakeAll();
        m_producerWaiter.wakeAll();
    }

    bool enough() const
    {
        QMutexLocker locker(&m_mutex);
        const int minFrames = 15;
        return m_packets.size() > minFrames && (!m_duration || m_duration > 1.0);
    }

    int bytes() const
    {
        QMutexLocker locker(&m_mutex);
        return m_bytes;
    }

    void clear()
    {
        QMutexLocker locker(&m_mutex);
        clearPackets();
    }

    void wake(bool wake)
    {
        QMutexLocker locker(&m_mutex);
        if (wake)
            m_consumerWaiter.wakeAll();
        m_wake = wake;
    }

protected:
    void clearPackets()
    {
        m_packets.clear();
        m_bytes = 0;
        m_duration = 0;
    }

    QList<QAVPacket> m_packets;
    mutable QMutex m_mutex;
    QWaitCondition m_consumerWaiter;
    QWaitCondition m_producerWaiter;
    bool m_abort = false;
    bool m_waitingForPackets = true;
    bool m_wake = false;

    int m_bytes = 0;
    int m_duration = 0;

private:
    Q_DISABLE_COPY(QAVPacketQueue)
};


QT_END_NAMESPACE

#endif
