#pragma once

#include <QMap>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QElapsedTimer>

namespace RM {

/// 数据新鲜度等级
enum class DataFreshness {
    Fresh,      // < threshold: 正常使用
    Degraded,   // 降低可信度 (×0.7)
    Stale,      // UI 灰化 (×0.4)
    Expired     // 不参与强推荐 (×0)
};

/// 新鲜度查询结果
struct FreshnessResult {
    DataFreshness level = DataFreshness::Expired;
    qint64 ageMs = 999999;
    double weightFactor = 0.0;
};

/// 数据时效性校验器。
/// 记录各数据源的最后更新时间戳, 查询时返回新鲜度等级和权重系数。
class DataFreshnessGuard : public QObject {
    Q_OBJECT

public:
    explicit DataFreshnessGuard(QObject *parent = nullptr);

    /// 设置新鲜度阈值 (毫秒)
    void setThresholds(qint64 freshMs, qint64 degradedMs, qint64 staleMs);

    /// 记录当前时间作为某数据源的最后更新时刻
    void touch(const QString &sourceId);
    /// 使用指定时间戳记录
    void touch(const QString &sourceId, qint64 timestampMs);

    /// 查询单个数据源新鲜度 (相对当前时间)
    FreshnessResult check(const QString &sourceId) const;
    /// 查询 — 使用指定参考时间戳
    FreshnessResult checkAt(const QString &sourceId, qint64 referenceMs) const;

    /// 返回所有已注册数据源的新鲜度
    QMap<QString, FreshnessResult> checkAll() const;

    /// 重置所有记录
    void reset();

    // 默认阈值常量
    static constexpr qint64 DEFAULT_FRESH_MS = 300;
    static constexpr qint64 DEFAULT_DEGRADED_MS = 800;
    static constexpr qint64 DEFAULT_STALE_MS = 1500;

private:
    FreshnessResult compute(qint64 ageMs) const;

    qint64 m_freshMs = DEFAULT_FRESH_MS;
    qint64 m_degradedMs = DEFAULT_DEGRADED_MS;
    qint64 m_staleMs = DEFAULT_STALE_MS;

    QMap<QString, qint64> m_timestamps;
    mutable QMutex m_mutex;
};

} // namespace RM
