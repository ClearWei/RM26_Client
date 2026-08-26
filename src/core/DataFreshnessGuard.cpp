#include "DataFreshnessGuard.h"
#include <QDateTime>
#include <algorithm>

namespace RM {

DataFreshnessGuard::DataFreshnessGuard(QObject *parent) : QObject(parent) {}

void DataFreshnessGuard::setThresholds(qint64 freshMs, qint64 degradedMs, qint64 staleMs) {
    QMutexLocker lock(&m_mutex);
    m_freshMs = freshMs;
    m_degradedMs = degradedMs;
    m_staleMs = staleMs;
}

void DataFreshnessGuard::touch(const QString &sourceId) {
    touch(sourceId, QDateTime::currentMSecsSinceEpoch());
}

void DataFreshnessGuard::touch(const QString &sourceId, qint64 timestampMs) {
    QMutexLocker lock(&m_mutex);
    m_timestamps[sourceId] = timestampMs;
}

FreshnessResult DataFreshnessGuard::check(const QString &sourceId) const {
    return checkAt(sourceId, QDateTime::currentMSecsSinceEpoch());
}

FreshnessResult DataFreshnessGuard::checkAt(const QString &sourceId, qint64 referenceMs) const {
    QMutexLocker lock(&m_mutex);
    if (!m_timestamps.contains(sourceId)) {
        FreshnessResult r;
        r.level = DataFreshness::Expired;
        r.ageMs = 999999;
        r.weightFactor = 0.0;
        return r;
    }
    qint64 ageMs = std::max(qint64(0), referenceMs - m_timestamps.value(sourceId));
    return compute(ageMs);
}

QMap<QString, FreshnessResult> DataFreshnessGuard::checkAll() const {
    QMap<QString, FreshnessResult> results;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QMutexLocker lock(&m_mutex);
    for (auto it = m_timestamps.cbegin(); it != m_timestamps.cend(); ++it) {
        qint64 ageMs = std::max(qint64(0), now - it.value());
        results[it.key()] = compute(ageMs);
    }
    return results;
}

void DataFreshnessGuard::reset() {
    QMutexLocker lock(&m_mutex);
    m_timestamps.clear();
}

FreshnessResult DataFreshnessGuard::compute(qint64 ageMs) const {
    FreshnessResult r;
    r.ageMs = ageMs;
    if (ageMs < m_freshMs) {
        r.level = DataFreshness::Fresh;
        r.weightFactor = 1.0;
    } else if (ageMs < m_degradedMs) {
        r.level = DataFreshness::Degraded;
        r.weightFactor = 0.7;
    } else if (ageMs < m_staleMs) {
        r.level = DataFreshness::Stale;
        r.weightFactor = 0.4;
    } else {
        r.level = DataFreshness::Expired;
        r.weightFactor = 0.0;
    }
    return r;
}

} // namespace RM
