#include "statisticswidget.h"
#include "mainwindow.h"
#include <QHBoxLayout>
#include <QGridLayout>


StatisticsWidget::StatisticsWidget(QWidget* parent)
    : QWidget(parent)
    , m_totalRecords(0)
    , m_matchCount(0)
    , m_mismatchCount(0)
    , m_missingCount(0)
    , m_pendingCount(0)
    , m_configOnlyCount(0)
    , m_filterCombo(nullptr)
    , m_filterLabel(nullptr)
    , m_currentFilter("All")
{
    setObjectName("StatisticsWidget");
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumHeight(80);

    // Color definitions
    QString matchColor = "#22C55E";
    QString mismatchColor = "#EF4444";
    QString missingColor = "#F59E0B";
    QString pendingColor = "#6B7280";
    QString totalColor = "#818CF8";
    QString configOnlyColor = "#A78BFA";
    QString successColor = "#10B981";

    // Helper lambda to create a stat card
    auto makeStatCard = [this](QLabel*& valLabel, const QString& title, const QString& color) -> QFrame* {
        QFrame* card = new QFrame(this);
        card->setObjectName("statCard");
        card->setStyleSheet(
            QString("QFrame#statCard {"
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "      stop:0 rgba(%1, 0.08), stop:1 rgba(%1, 0.02));"
            "  border: 1px solid rgba(%1, 0.25);"
            "  border-radius: 10px;"
            "  padding: 6px 14px;"
            "  min-width: 60px;"
            "}").arg(color)
        );

        QVBoxLayout* layout = new QVBoxLayout(card);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(2);
        layout->setAlignment(Qt::AlignCenter);

        QLabel* titleLabel = new QLabel(title, card);
        titleLabel->setStyleSheet(
            QString("color: %1; font-size: 9px; font-weight: 700; letter-spacing: 1px;").arg(color));
        titleLabel->setAlignment(Qt::AlignCenter);

        valLabel = new QLabel("0", card);
        valLabel->setStyleSheet(
            QString("color: %1; font-size: 22px; font-weight: 800;").arg(color));
        valLabel->setAlignment(Qt::AlignCenter);

        layout->addWidget(titleLabel);
        layout->addWidget(valLabel);
        return card;
    };

    // Main horizontal layout
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(10, 6, 10, 6);

    // Stat cards
    QFrame* totalCard = makeStatCard(m_totalRecordsLabel, "TOTAL", totalColor);
    QFrame* matchCard = makeStatCard(m_matchCountLabel, "MATCH", matchColor);
    QFrame* mismatchCard = makeStatCard(m_mismatchCountLabel, "MISMATCH", mismatchColor);
    QFrame* missingCard = makeStatCard(m_missingCountLabel, "MISSING", missingColor);
    QFrame* configOnlyCard = makeStatCard(m_configOnlyCountLabel, "CONFIG", configOnlyColor);
    QFrame* pendingCard = makeStatCard(m_pendingCountLabel, "PENDING", pendingColor);

    mainLayout->addWidget(totalCard);
    mainLayout->addWidget(matchCard);
    mainLayout->addWidget(mismatchCard);
    mainLayout->addWidget(missingCard);
    mainLayout->addWidget(configOnlyCard);
    mainLayout->addWidget(pendingCard);

    // Spacer before controls
    mainLayout->addStretch();

    // Controls: Filter + CRC-32 toggle
    QVBoxLayout* controlLayout = new QVBoxLayout();
    controlLayout->setSpacing(6);

    // Filter row
    QHBoxLayout* filterRow = new QHBoxLayout();
    filterRow->setSpacing(6);
    m_filterLabel = new QLabel("Filter:", this);
    m_filterLabel->setStyleSheet("color: #94A3B8; font-size: 11px; font-weight: 600;");
    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItems({"All", "Match", "Mismatch", "Missing", "Pending", "Config-Only"});
    m_filterCombo->setCurrentText("All");
    m_filterCombo->setStyleSheet(
        "QComboBox { background: #1E293B; color: #E2E8F0; border: 1px solid #334155;"
        " border-radius: 6px; padding: 4px 8px; font-size: 11px; font-weight: 600; min-height: 24px; }"
        "QComboBox::drop-down { border: none; padding-right: 4px; }"
        "QComboBox QAbstractItemView { background: #1E293B; color: #E2E8F0;"
        " selection-background-color: #334155; border: 1px solid #334155; }"
    );
    connect(m_filterCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged), this, [this](const QString& text) {
        m_currentFilter = text;
        if (m_filterCallback) {
            m_filterCallback(text);
        }
        updateDisplay();
    });
    filterRow->addWidget(m_filterLabel);
    filterRow->addWidget(m_filterCombo);
    controlLayout->addLayout(filterRow);

    // CRC-32 toggle
    m_crc32Checkbox = new QCheckBox("Show CRC-32", this);
    m_crc32Checkbox->setChecked(true);
    m_crc32Checkbox->setStyleSheet(
        "QCheckBox { color: #E2E8F0; font-size: 11px; font-weight: 600; spacing: 4px; }"
        "QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #334155; border-radius: 4px; background: #1E293B; }"
        "QCheckBox::indicator:checked { background: #6366F1; border: 1px solid #6366F1; }"
    );
    connect(m_crc32Checkbox, &QCheckBox::toggled, this, &StatisticsWidget::crc32Toggled);
    controlLayout->addWidget(m_crc32Checkbox);

    mainLayout->addLayout(controlLayout);
}

void StatisticsWidget::updateStats(const QMap<int, VddRecord>& records) {
    m_totalRecords = records.size();
    m_matchCount = 0;
    m_mismatchCount = 0;
    m_missingCount = 0;
    m_pendingCount = 0;
    m_configOnlyCount = 0;

    for (const VddRecord& rec : records) {
        QString status = rec.localStatus.toUpper();
        if (status == "MATCH") {
            m_matchCount++;
        } else if (status == "MISMATCH") {
            m_mismatchCount++;
        } else if (status == "MISSING") {
            m_missingCount++;
        } else if (status == "CONFIG_ONLY") {
            m_configOnlyCount++;
        } else if (status == "PENDING" || status == "ERROR") {
            m_pendingCount++;
        }
    }

    updateDisplay();
}

void StatisticsWidget::updateDisplay() {
    int totalVisible = m_totalRecords;
    QString filter = m_currentFilter;

    if (filter == "Match") {
        totalVisible = m_matchCount;
    } else if (filter == "Mismatch") {
        totalVisible = m_mismatchCount;
    } else if (filter == "Missing") {
        totalVisible = m_missingCount;
    } else if (filter == "Pending") {
        totalVisible = m_pendingCount;
    } else if (filter == "Config-Only") {
        totalVisible = m_configOnlyCount;
    }

    m_totalRecordsLabel->setText(QString::number(totalVisible));
    m_matchCountLabel->setText(QString::number(m_matchCount));
    m_mismatchCountLabel->setText(QString::number(m_mismatchCount));
    m_missingCountLabel->setText(QString::number(m_missingCount));
    m_pendingCountLabel->setText(QString::number(m_pendingCount));
    m_configOnlyCountLabel->setText(QString::number(m_configOnlyCount));

}

QString StatisticsWidget::formatPercentage(int value, int total) const {
    if (total == 0) return "0%";
    return QString::asprintf("%.1f%%", (value * 100.0) / total);
}

void StatisticsWidget::reset() {
    m_totalRecords = 0;
    m_matchCount = 0;
    m_mismatchCount = 0;
    m_missingCount = 0;
    m_pendingCount = 0;
    m_configOnlyCount = 0;
    m_currentFilter = "All";
    m_filterCombo->setCurrentText("All");
    updateDisplay();
}
