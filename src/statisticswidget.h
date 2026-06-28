#ifndef STATISTICSWIDGET_H
#define STATISTICSWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QFrame>
#include <QProgressBar>
#include <functional>

// Forward declaration
struct VddRecord;

/**
 * @brief The StatisticsWidget class provides a real-time dashboard
 * showing audit statistics and status distribution.
 */
class StatisticsWidget : public QWidget {
    Q_OBJECT
public:
    explicit StatisticsWidget(QWidget* parent = nullptr);
    ~StatisticsWidget() override = default;

    /**
     * @brief Update statistics with current records
     * @param records Map of record ID to VddRecord data
     */
    void updateStats(const QMap<int, VddRecord>& records);

    /**
     * @brief Reset all statistics to zero
     */
    void reset();

    /**
     * @brief Set callback to apply table row visibility based on filter
     */
    void setFilterCallback(std::function<void(const QString&)> cb) { m_filterCallback = cb; }

signals:
    void crc32Toggled(bool checked);

private:
    void updateDisplay();
    QString formatPercentage(int value, int total) const;

    // Statistics labels
    QLabel* m_totalRecordsLabel;
    QLabel* m_matchCountLabel;
    QLabel* m_mismatchCountLabel;
    QLabel* m_missingCountLabel;
    QLabel* m_pendingCountLabel;
    QLabel* m_configOnlyCountLabel;

    // Current statistics
    int m_totalRecords;
    int m_matchCount;
    int m_mismatchCount;
    int m_missingCount;
    int m_pendingCount;
    int m_configOnlyCount;

    // Filter dropdown
    QComboBox* m_filterCombo;
    QLabel* m_filterLabel;
    QString m_currentFilter;

    // CRC-32 toggle checkbox (placed beside filter)
    QCheckBox* m_crc32Checkbox;

    // Callback to apply filter to table
    std::function<void(const QString&)> m_filterCallback;
};

#endif // STATISTICSWIDGET_H
