#include "mainwindow.h"
#include "aiconfigmanager.h"
#include "checksumworker.h"
#include "statisticswidget.h"
#include "bulkchecksumdock.h"
#include "vdddocumentreviewer.h"
#include "zipreader.h"
#include "odtextractor.h"

#include <QRegularExpression>
#include "iconhelper.h"

#include <QApplication>
#include <QIcon>
// ODT extraction uses our own OdtExtractor class (no Qt private API)
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFrame>
#include <QEvent>
#include <QFileDialog>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

#include <QAction>
#include <QToolBar>
#include <QMenu>
#include <QToolButton>
#include <QFormLayout>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>

// ============================================================================
// Sleek Modal Settings Dialog Implementation
// ============================================================================
SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("AI Gateway Integration Settings");
    setWindowIcon(QIcon(":/icons/AI.ico"));
    setMinimumWidth(450);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(12);

    QLabel* titleLabel = new QLabel("Anthropic LLM Gateway Credentials & Parameters", this);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #6366F1; margin-bottom: 8px;");
    mainLayout->addWidget(titleLabel);

    QFormLayout* formLayout = new QFormLayout();
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignLeft);

    m_endpointEdit = new QLineEdit(this);
    m_endpointEdit->setPlaceholderText("Enter Anthropic gateway URL endpoint...");
    formLayout->addRow("Gateway URL:", m_endpointEdit);
    
    m_accessTokenEdit = new QLineEdit(this);
    m_accessTokenEdit->setEchoMode(QLineEdit::Password);
    m_accessTokenEdit->setPlaceholderText("API Auth Access Token...");
    formLayout->addRow("API Access Token:", m_accessTokenEdit);

    QHBoxLayout* tempLayout = new QHBoxLayout();
    m_tempSlider = new QSlider(Qt::Horizontal, this);
    m_tempSlider->setRange(0, 100);
    tempLayout->addWidget(m_tempSlider);
    m_tempValueLabel = new QLabel("0.0", this);
    m_tempValueLabel->setMinimumWidth(30);
    tempLayout->addWidget(m_tempValueLabel);
    connect(m_tempSlider, &QSlider::valueChanged, this, [this](int val) {
        m_tempValueLabel->setText(QString::asprintf("%.2f", val / 100.0));
    });
    formLayout->addRow("Temperature:", tempLayout);

    m_tokenLimitEdit = new QLineEdit(this);
    m_tokenLimitEdit->setPlaceholderText("4096");
    formLayout->addRow("Max Tokens Limit:", m_tokenLimitEdit);

    QHBoxLayout* modelLayout = new QHBoxLayout();
    m_modelCombo = new QComboBox(this);
    m_modelCombo->setMinimumWidth(180);
    modelLayout->addWidget(m_modelCombo, 1);
    m_discoverBtn = new QPushButton("Discover Models", this);
    m_discoverBtn->setIcon(QIcon(":/icons/refresh.svg"));
    m_discoverBtn->setIconSize(QSize(14, 14));
    m_discoverBtn->setStyleSheet("padding: 6px 12px; font-size: 11px;");
    modelLayout->addWidget(m_discoverBtn);
    formLayout->addRow("Target Model:", modelLayout);

    mainLayout->addLayout(formLayout);

    // Save & Cancel Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    QPushButton* cancelBtn = new QPushButton("Cancel", this);
    cancelBtn->setIcon(QIcon(":/icons/error.svg"));
    cancelBtn->setIconSize(QSize(14, 14));
    cancelBtn->setStyleSheet("background-color: #374151; color: white; padding: 6px 12px; font-weight: bold;");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    m_saveBtn = new QPushButton("Save Settings", this);
    m_saveBtn->setIcon(QIcon(":/icons/save.svg"));
    m_saveBtn->setIconSize(QSize(14, 14));
    m_saveBtn->setStyleSheet("background-color: #10B981; color: white; font-weight: bold; padding: 6px 16px;");
    connect(m_saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSaveSettingsClicked);

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(m_saveBtn);
    mainLayout->addLayout(btnLayout);

    // Wire up AI configuration callbacks
    connect(AIConfigManager::getInstance(), &AIConfigManager::modelsDiscovered, this, &SettingsDialog::onModelsPopulated);
    connect(AIConfigManager::getInstance(), &AIConfigManager::discoveryFailed, this, &SettingsDialog::onModelDiscoveryFailed);
    connect(m_discoverBtn, &QPushButton::clicked, this, &SettingsDialog::onDiscoverModelsClicked);

    // Synchronize initial parameters
    AIConfigManager* config = AIConfigManager::getInstance();
    m_endpointEdit->setText(config->getGatewayUrl());
    m_accessTokenEdit->setText(config->getAccessToken());
    m_tempSlider->setValue(static_cast<int>(config->getTemperature() * 100));
    m_tempValueLabel->setText(QString::asprintf("%.2f", config->getTemperature()));
    m_tokenLimitEdit->setText(QString::asprintf("%d", config->getMaxTokens()));
    m_modelCombo->addItem(config->getTargetModel());
    m_modelCombo->setCurrentText(config->getTargetModel());
}

void SettingsDialog::onDiscoverModelsClicked() {
    m_discoverBtn->setEnabled(false);
    // Persist the current URL field value before discovery so discoverModels() reads the right endpoint
    AIConfigManager::getInstance()->setGatewayUrl(m_endpointEdit->text().trimmed());
    AIConfigManager::getInstance()->discoverModels();
}

void SettingsDialog::onModelsPopulated(const QStringList& models) {
    m_discoverBtn->setEnabled(true);
    m_modelCombo->clear();
    m_modelCombo->addItems(models);
}

void SettingsDialog::onModelDiscoveryFailed(const QString& errMsg) {
    m_discoverBtn->setEnabled(true);
    QMessageBox::warning(this, "Discovery Failed", "Cannot discover models dynamically: " + errMsg);
}

void SettingsDialog::onSaveSettingsClicked() {
    AIConfigManager* config = AIConfigManager::getInstance();
    config->setGatewayUrl(m_endpointEdit->text().trimmed());
    config->setAccessToken(m_accessTokenEdit->text().trimmed());
    config->setTemperature(m_tempSlider->value() / 100.0);
    config->setTargetModel(m_modelCombo->currentText().trimmed());
    
    bool isInt = false;
    int limit = m_tokenLimitEdit->text().toInt(&isInt);
    if (isInt) {
        config->setMaxTokens(limit);
    }
    
    accept();
}

// ============================================================================
// MainWindow Implementation
// ============================================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tableStack(nullptr)
    , m_welcomeWidget(nullptr)
    , m_tableWidget(nullptr)
    , m_statisticsWidget(nullptr)
    , m_verifyLocalBtn(nullptr)
    , m_importVddBtn(nullptr)
    , m_exportResultsBtn(nullptr)
    , m_configFileEdit(nullptr)
    , m_localProgressBar(nullptr)
    , m_statusLabel(nullptr)
    , m_llmImportTargetId(1)
    , m_llmParseMode("")
    , m_activeHashJobs(0)
    , m_completedHashJobs(0)
    , m_totalHashFiles(0)
{
    // Progress throttling via QTimer — worker threads flood signals, Qt drops most.
    // We use a single-shot fire-and-forget pattern: each file emits updates,
    // but we only apply them to the overall progress bar every 100ms.
    m_progressTimer = new QTimer(this);
    m_progressTimer->setInterval(100);
    connect(m_progressTimer, &QTimer::timeout, this, &MainWindow::updateProgressSlow);
    m_progressTimer->start();
    initUi();
    loadStyleSheet();
    setAcceptDrops(true);

    // LLM query callbacks
    connect(AIConfigManager::getInstance(), &AIConfigManager::queryStarted, this, &MainWindow::onLLMQueryStarted);
    connect(AIConfigManager::getInstance(), &AIConfigManager::queryProgress, this, &MainWindow::onLLMQueryProgress);
    connect(AIConfigManager::getInstance(), &AIConfigManager::queryResult, this, &MainWindow::onLLMQueryResult);
    connect(AIConfigManager::getInstance(), &AIConfigManager::queryFailed, this, &MainWindow::onLLMQueryFailed);

    m_statusLabel->setText("System initialized. Drop `.odt` VDD files to begin automated extraction and verification.");

    // Initialize network manager
    m_networkManager = new QNetworkAccessManager(this);

    // Initialize config file paths (empty initially)
    m_csvFilePaths.clear();

    // Automatically trigger guided tour at startup (300ms delay to let layout compute)
    QTimer::singleShot(300, this, &MainWindow::startGuidedTour);
}

MainWindow::~MainWindow() {
    if (m_progressTimer) m_progressTimer->stop();
    // Terminate any active threads safely with 5-second timeout
    for (QThread* thread : m_verifierThreads) {
        if (thread->isRunning()) {
            thread->quit();
            if (!thread->wait(5000)) {  // 5 second timeout
                thread->terminate();
                thread->wait();
            }
        }
    }
}

void MainWindow::initUi() {
    setWindowTitle("VDD (Version Description Document) Automated Audit & Verification System");

    // --- Main tab widget with 3 tabs: Home, Bulk Checksum, Document Review ---
    m_mainTabWidget = new QTabWidget(this);
    m_mainTabWidget->setDocumentMode(true);
    // m_mainTabWidget->setExpanding(false); // not a real QTabWidget method
    setCentralWidget(m_mainTabWidget);

    // ========================
    // Tab 0: Home (Audit Dashboard)
    // ========================
    m_homePage = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(m_homePage);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(12);

    // 0. Quick Actions Toolbar
    QToolBar* toolBar = addToolBar("Quick Actions");
    toolBar->setMovable(false);
    toolBar->setStyleSheet(
        "QToolBar { background: rgba(15, 23, 42, 0.8); spacing: 4px; padding: 4px;"
        "  border-bottom: 2px solid qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "      stop:0 rgba(99,102,241,0.6), stop:0.5 rgba(139,92,246,0.4), stop:1 rgba(99,102,241,0.1)); }"
        "QToolBar::separator { background: rgba(99, 102, 241, 0.3); width: 1px; margin: 4px; }"
    );

    // Settings Action (Gear Icon)
    QAction* settingsAction = new QAction("Settings", this);
    settingsAction->setIcon(QIcon(":/icons/settings.svg"));
    settingsAction->setToolTip("AI Gateway Integration Credentials");
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onOpenSettingsClicked);
    toolBar->addAction(settingsAction);

    toolBar->addSeparator();

    // Export button with dropdown menu
    QToolButton* exportBtn = new QToolButton(this);
    exportBtn->setIcon(QIcon(":/icons/export.ico"));
    exportBtn->setToolTip("Export Options");
    exportBtn->setStyleSheet("QToolButton { border: none; padding: 4px; }");

    QMenu* exportMenu = new QMenu(this);
    exportMenu->setStyleSheet(
        "QMenu { background: rgba(15, 23, 42, 0.95); border: 1px solid rgba(99, 102, 241, 0.3); border-radius: 8px; }"
        "QMenu::item { padding: 8px 16px; color: #F8FAFC; }"
        "QMenu::item:selected { background: rgba(99, 102, 241, 0.2); }"
    );
    exportMenu->addAction("Export to CSV", this, &MainWindow::onExportToCsvClicked);
    exportMenu->addAction("Generate Report", this, &MainWindow::onGeneratePdfClicked);
    exportBtn->setMenu(exportMenu);
    exportBtn->setPopupMode(QToolButton::InstantPopup);

    toolBar->addWidget(exportBtn);

    toolBar->addSeparator();

    // Refresh Action
    QAction* refreshAction = new QAction("Reset", this);
    refreshAction->setIcon(QIcon(":/icons/reset.ico"));
    refreshAction->setToolTip("Reset Table Display");
    connect(refreshAction, &QAction::triggered, this, [this]() {
        // Clear all data
        m_records.clear();
        m_csvFilePaths.clear();
        m_configFileEdit->clear();
        m_statisticsWidget->reset();
        m_activeHashJobs = 0;
        m_completedHashJobs = 0;
        m_totalHashFiles = 0;
        m_fileProgressMap.clear();
        m_rowExpanded.clear();
        m_mainRowToDetailRow.clear();
        updateTableDisplay();
        m_statusLabel->setText("Ready");
        logMessage("Table display and data cleared", "info");
    });
    toolBar->addAction(refreshAction);

    toolBar->addSeparator();




    // 1. Top Panel: Sleek Workspace controller occupying full top screen width
    QGroupBox* workspaceGroup = new QGroupBox("Audit Files & Environment Settings", this);
    QGridLayout* workspaceLayout = new QGridLayout(workspaceGroup);
    workspaceLayout->setSpacing(10);

    workspaceLayout->addWidget(new QLabel("Configuration Items List (CSV):", this), 0, 0);
    m_configFileEdit = new QLineEdit("", this);
    m_configFileEdit->setPlaceholderText("Browse for CSV file listing configuration items...");
    m_configFileEdit->setReadOnly(true);
    workspaceLayout->addWidget(m_configFileEdit, 0, 1);
    QPushButton* browseConfigBtn = new QPushButton("Browse...", this);
    browseConfigBtn->setIcon(QIcon(":/icons/browse.svg"));
    browseConfigBtn->setIconSize(QSize(14, 14));
    connect(browseConfigBtn, &QPushButton::clicked, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Select Configuration Items File", "",
            "CSV Files (*.csv);;All Files (*.*)");
        if (!filePath.isEmpty()) {
            // Replace previous config file and clear all verification results
            m_csvFilePaths.clear();
            m_csvFilePaths.append(filePath);

            // Clear all existing record data so next verify starts fresh
            for (auto it = m_records.begin(); it != m_records.end(); ++it) {
                VddRecord& rec = it.value();
                rec.localFileName.clear();
                rec.localCiRef.clear();
                rec.localVersion.clear();
                rec.localFullPath.clear();
                rec.configFileName.clear();
                rec.configVersion.clear();
                rec.configPath.clear();
                rec.configComponent.clear();
                rec.configPathExists = false;
                rec.configFileFoundAtPath = false;
                rec.localStatus.clear();
                rec.localStatusReason.clear();
                rec.fileStatus.clear();
                rec.fileStatusReason.clear();
                rec.calculatedMd5.clear();
                rec.calculatedSha1.clear();
                rec.calculatedCrc32.clear();
                rec.ciRefCheck = "SKIP";
                rec.compCheck = "SKIP";
                rec.versionCheck = "SKIP";
                rec.md5Check = "SKIP";
                rec.pathCheck = "SKIP";
                rec.crc32Check = "SKIP";
            }

            // Remove CONFIG_ONLY records from the previous run — they will be
            // regenerated fresh when "Verify All" is clicked with the new CSV.
            {
                QList<int> configOnlyIds;
                for (int id : m_records.keys()) {
                    if (m_records[id].source == "CONFIG_ONLY")
                        configOnlyIds.append(id);
                }
                for (int id : configOnlyIds)
                    m_records.remove(id);
            }

            // If there are no VDD records, treat this as a config-only browse
            if (m_records.isEmpty()) {
                m_tableWidget->setRowCount(0);
            } else {
                // Re-verify existing VDD records with the new config file
                updateTableDisplay();
            }

            m_configFileEdit->setText(QFileInfo(filePath).fileName() + " (" + filePath + ")");
            m_statusLabel->setText(QString::asprintf("Loaded new config file '%s' — verification results cleared, VDD data preserved. Click 'Verify All' to re-run.", QFileInfo(filePath).fileName().toLocal8Bit().constData()));
            // Enable Verify All as soon as a CSV is loaded — even without a VDD import.
            if (m_verifyLocalBtn) m_verifyLocalBtn->setEnabled(true);
        }
    });
    workspaceLayout->addWidget(browseConfigBtn, 0, 2);

    // Ingestion & Audit Buttons block
    QHBoxLayout* btnRow = new QHBoxLayout();
    
    m_importVddBtn = new QPushButton("Import & Analyze VDD (.odt)...", this);
    m_importVddBtn->setObjectName("importVddBtn");
    m_importVddBtn->setIcon(QIcon(":/icons/import.svg"));
    m_importVddBtn->setIconSize(QSize(16, 16));
    m_importVddBtn->setCursor(Qt::PointingHandCursor);
    connect(m_importVddBtn, &QPushButton::clicked, this, &MainWindow::onImportVddClicked);

    m_verifyLocalBtn = new QPushButton("Verify All", this);
    m_verifyLocalBtn->setObjectName("verifyLocalBtn");
    m_verifyLocalBtn->setIcon(QIcon(":/icons/verify.svg"));
    m_verifyLocalBtn->setIconSize(QSize(16, 16));
    m_verifyLocalBtn->setCursor(Qt::PointingHandCursor);
    m_verifyLocalBtn->setEnabled(false); // Disabled by default until records are loaded
    connect(m_verifyLocalBtn, &QPushButton::clicked, this, &MainWindow::onVerifyLocalClicked);

    m_exportResultsBtn = new QPushButton("Export Results", this);
    m_exportResultsBtn->setObjectName("exportResultsBtn");
    m_exportResultsBtn->setIcon(QIcon(":/icons/export.ico"));
    m_exportResultsBtn->setToolTip("Export to CSV");
    m_exportResultsBtn->setIconSize(QSize(16, 16));
    m_exportResultsBtn->setCursor(Qt::PointingHandCursor);
    connect(m_exportResultsBtn, &QPushButton::clicked, this, &MainWindow::onExportToCsvClicked);

    btnRow->addWidget(m_importVddBtn);
    btnRow->addWidget(m_verifyLocalBtn);
    btnRow->addWidget(m_exportResultsBtn);
    workspaceLayout->addLayout(btnRow, 1, 0, 1, 4);

    mainLayout->addWidget(workspaceGroup);

    // 2. Statistics Dashboard Panel
    m_statisticsWidget = new StatisticsWidget(this);
    mainLayout->addWidget(m_statisticsWidget);

    // Connect CRC-32 toggle signal from StatisticsWidget
    connect(m_statisticsWidget, &StatisticsWidget::crc32Toggled, this, &MainWindow::onCrc32Toggled);

    // Connect filter callback to hide/show table rows based on filter selection
    m_statisticsWidget->setFilterCallback([this](const QString& filter) {
        // Restore row spans from previous collapsed state
        for (auto it = m_rowExpanded.begin(); it != m_rowExpanded.end(); ++it) {
            int mainRow = it.key();
            bool expanded = it.value();
            if (expanded && m_mainRowToDetailRow.contains(mainRow)) {
                int detailRow = m_mainRowToDetailRow[mainRow];
                m_tableWidget->setRowHidden(detailRow, false);
            }
        }

        int tableRowCount = m_tableWidget->rowCount();
        // Collect record IDs for each row — check both fileName and configFileName
        // so CONFIG_ONLY rows (which display configFileName in col 0) are matched.
        QVector<int> rowRecordIds;
        rowRecordIds.resize(tableRowCount);
        for (int r = 0; r < tableRowCount; ++r) {
            QTableWidgetItem* fnItem = m_tableWidget->item(r, 0);
            if (!fnItem) { rowRecordIds[r] = -1; continue; }
            QString cellText = fnItem->text();
            int recordId = -1;
            for (int id : m_records.keys()) {
                const VddRecord& rec = m_records[id];
                if (rec.fileName == cellText ||
                    (rec.source == "CONFIG_ONLY" && rec.configFileName == cellText)) {
                    recordId = id;
                    break;
                }
            }
            rowRecordIds[r] = recordId;
        }

        for (int r = 0; r < tableRowCount; ++r) {
            if (rowRecordIds[r] < 0) {
                m_tableWidget->setRowHidden(r, false);
                continue;
            }
            int id = rowRecordIds[r];
            bool isDetailRow = m_mainRowToDetailRow.values().contains(r);
            if (isDetailRow) {
                // m_rowExpanded is keyed by main row index, not record ID
                int mainRow = -1;
                for (auto it = m_mainRowToDetailRow.begin(); it != m_mainRowToDetailRow.end(); ++it) {
                    if (it.value() == r) { mainRow = it.key(); break; }
                }
                m_tableWidget->setRowHidden(r, !(mainRow >= 0 && m_rowExpanded.value(mainRow, false)));
                continue;
            }
            const VddRecord& rec = m_records[id];
            bool visible = true;
            if (filter == "Match") visible = (rec.localStatus == "MATCH");
            else if (filter == "Mismatch") visible = (rec.localStatus == "MISMATCH");
            else if (filter == "Missing") visible = (rec.localStatus == "MISSING");
            else if (filter == "Pending") visible = (rec.localStatus == "PENDING" || rec.localStatus == "ERROR");
            else if (filter == "Config-Only") visible = (rec.localStatus == "CONFIG_ONLY");
            // "All" — visible
            m_tableWidget->setRowHidden(r, !visible);
        }
    });

    // 3. QStackedWidget container for Table/Welcome view
    m_tableStack = new QStackedWidget(this);

    // Create Welcome Splash Widget
    m_welcomeWidget = new QWidget(this);
    QVBoxLayout* welcomeLayout = new QVBoxLayout(m_welcomeWidget);
    welcomeLayout->setAlignment(Qt::AlignCenter);
    welcomeLayout->setContentsMargins(40, 40, 40, 40);
    welcomeLayout->setSpacing(20);

    QFrame* welcomeCard = new QFrame(m_welcomeWidget);
    welcomeCard->setObjectName("welcomeCard");
    welcomeCard->setMinimumSize(600, 360);
    welcomeCard->setMaximumSize(750, 480);
    welcomeCard->setStyleSheet(
        "QFrame#welcomeCard {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(15, 23, 42, 0.65), stop:1 rgba(30, 41, 59, 0.45));"
        "    border: 1px solid rgba(99, 102, 241, 0.25);"
        "    border-radius: 16px;"
        "    padding: 30px;"
        "}"
    );

    QVBoxLayout* cardLayout = new QVBoxLayout(welcomeCard);
    cardLayout->setAlignment(Qt::AlignCenter);
    cardLayout->setSpacing(16);

    // Large decorative Icon
    QLabel* welcomeIcon = new QLabel(welcomeCard);
    welcomeIcon->setPixmap(QIcon(":/icons/dashboard.svg").pixmap(64, 64));
    welcomeIcon->setAlignment(Qt::AlignCenter);
    welcomeIcon->setStyleSheet("background: transparent;");
    cardLayout->addWidget(welcomeIcon);

    QLabel* welcomeTitle = new QLabel("VDD Automated Audit & Verification", welcomeCard);
    welcomeTitle->setAlignment(Qt::AlignCenter);
    welcomeTitle->setStyleSheet(
        "QLabel {"
        "    font-size: 22px;"
        "    font-weight: bold;"
        "    color: #F8FAFC;"
        "    background: transparent;"
        "    letter-spacing: 0.5px;"
        "}"
    );
    cardLayout->addWidget(welcomeTitle);

    QLabel* welcomeText = new QLabel(
        "Welcome to the VDD Audit System. To begin, please select a task:\n\n"
        "1. Drag and drop any Version Description Document (.odt, .csv, .xlsx) here\n"
        "2. Click the 'Import VDD' button on the toolbar above to load a document\n"
        "3. Select a target directory to verify local file integrity against VDD references\n"
        "4. Switch to the 'Document Review' tab to perform LLM-assisted semantic quality audits.",
        welcomeCard
    );
    welcomeText->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    welcomeText->setWordWrap(true);
    welcomeText->setStyleSheet(
        "QLabel {"
        "    font-size: 13px;"
        "    color: #94A3B8;"
        "    line-height: 1.6;"
        "    background: transparent;"
        "    padding: 10px 20px;"
        "}"
    );
    cardLayout->addWidget(welcomeText);

    // Simple visual CTA button
    QPushButton* quickImportBtn = new QPushButton("Import VDD File Now", welcomeCard);
    quickImportBtn->setObjectName("quickImportBtn");
    quickImportBtn->setCursor(Qt::PointingHandCursor);
    quickImportBtn->setMinimumHeight(40);
    quickImportBtn->setMaximumWidth(220);
    quickImportBtn->setStyleSheet(
        "QPushButton#quickImportBtn {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #6366F1, stop:1 #8B5CF6);"
        "    color: #FFFFFF;"
        "    border: none;"
        "    border-radius: 8px;"
        "    font-size: 13px;"
        "    font-weight: bold;"
        "    padding: 8px 16px;"
        "}"
        "QPushButton#quickImportBtn:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4F46E5, stop:1 #7C3AED);"
        "}"
        "QPushButton#quickImportBtn:pressed {"
        "    background: #4338CA;"
        "}"
    );
    connect(quickImportBtn, &QPushButton::clicked, this, &MainWindow::onImportVddClicked);
    cardLayout->addWidget(quickImportBtn, 0, Qt::AlignCenter);

    welcomeLayout->addWidget(welcomeCard, 0, Qt::AlignCenter);

    // 3. QTableWidget Verification Dashboard
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(20);
    QStringList headers;
    headers << "File Name (VDD)" << "File Name (Local Dir)" << "File Name Result"
            << "CI Ref (VDD)" << "CI Ref (Local Dir)" << "CI Ref (Config List)" << "CI Ref Result"
            << "Version (VDD)" << "Version (Local Dir)" << "Version (Config List)" << "Version Result"
            << "MD5 (VDD)" << "MD5 (Local Dir)" << "MD5 Result"
            << "CRC-32 (VDD)" << "CRC-32 (Local Dir)" << "CRC-32 Result"
            << "Document Link" << "Doc Link Result" << "Source";
    m_tableWidget->setHorizontalHeaderLabels(headers);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setContextMenuPolicy(Qt::PreventContextMenu);
    m_tableWidget->horizontalHeader()->setSortIndicatorShown(false);
    m_tableWidget->horizontalHeader()->setSectionsClickable(false);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tableWidget->setSortingEnabled(false);

    // Connect cell click signal for expand/collapse functionality
    connect(m_tableWidget, &QTableWidget::cellClicked, this, &MainWindow::toggleRowDetails);
    connect(m_tableWidget, &QTableWidget::customContextMenuRequested, this, &MainWindow::onCustomContextMenuRequested);

    m_tableWidget->setColumnWidth(0, 180);  // File Name (VDD)
    m_tableWidget->setColumnWidth(1, 180);  // File Name (Local Dir)
    m_tableWidget->setColumnWidth(2, 90);   // File Name Result
    m_tableWidget->setColumnWidth(3, 150);  // CI Ref (VDD)
    m_tableWidget->setColumnWidth(4, 150);  // CI Ref (Local Dir)
    m_tableWidget->setColumnWidth(5, 150);  // CI Ref (Config List)
    m_tableWidget->setColumnWidth(6, 100);  // CI Ref Result
    m_tableWidget->setColumnWidth(7, 100);  // Version (VDD)
    m_tableWidget->setColumnWidth(8, 120);  // Version (Local Dir)
    m_tableWidget->setColumnWidth(9, 120);  // Version (Config List)
    m_tableWidget->setColumnWidth(10, 90);  // Version Result
    m_tableWidget->setColumnWidth(11, 120); // MD5 (VDD)
    m_tableWidget->setColumnWidth(12, 140); // MD5 (Local Dir)
    m_tableWidget->setColumnWidth(13, 90);  // MD5 Result
    m_tableWidget->setColumnWidth(14, 120); // CRC-32 (VDD)
    m_tableWidget->setColumnWidth(15, 140); // CRC-32 (Local Dir)
    m_tableWidget->setColumnWidth(16, 90);  // CRC-32 Result
    m_tableWidget->setColumnWidth(17, 200); // Document Link
    m_tableWidget->setColumnWidth(18, 90);  // Doc Link Result
    m_tableWidget->setColumnWidth(19, 90);  // Source

    m_tableStack->addWidget(m_welcomeWidget);
    m_tableStack->addWidget(m_tableWidget);
    m_tableStack->setCurrentIndex(0); // Welcome splash shown initially

    mainLayout->addWidget(m_tableStack);

    // 3. Bottom Section: Status bar
    QHBoxLayout* bottomLayout = new QHBoxLayout();

    m_statusLabel = new QLabel("System idle.", this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("color: #A1A1AA; font-style: italic; border-left: 2px solid #555; padding-left: 10px;");
    bottomLayout->addWidget(m_statusLabel);
    bottomLayout->addStretch();

    // Verification progress bar — wider, more prominent
    m_localProgressBar = new QProgressBar(this);
    m_localProgressBar->setRange(0, 100);
    m_localProgressBar->setValue(0);
    m_localProgressBar->setMaximumHeight(32);
    m_localProgressBar->setMinimumWidth(500);
    m_localProgressBar->setTextVisible(true);
    m_localProgressBar->setAlignment(Qt::AlignCenter);
    m_localProgressBar->setStyleSheet(
        "QProgressBar {"
        "    background: rgba(30, 41, 59, 0.95);"
        "    border: 2px solid rgba(99, 102, 241, 0.5);"
        "    border-radius: 16px;"
        "    text-align: center;"
        "    font-size: 13px;"
        "    font-weight: 700;"
        "    color: #E2E8F0;"
        "}"
        "QProgressBar::chunk {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "        stop:0 #6366F1, stop:0.5 #818CF8, stop:1 #A78BFA);"
        "    border-radius: 14px;"
        "}"
    );
    bottomLayout->addWidget(m_localProgressBar);

    mainLayout->addLayout(bottomLayout);

    // 4. Action Log Dock Widget (IDE-style bottom console logs)
    m_actionLogDock = new QDockWidget("System Verification Log", this);
    m_actionLogDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_actionLogText = new QTextEdit(this);
    m_actionLogText->setReadOnly(true);
    m_actionLogText->setMinimumHeight(140);
    m_actionLogText->setStyleSheet(
        "QTextEdit { background: #070A13; border: 1px solid rgba(99, 102, 241, 0.2); border-radius: 8px; font-family: 'Consolas', 'Monaco', monospace; font-size: 11px; padding: 6px; color: #F8FAFC; }"
    );
    m_actionLogDock->setWidget(m_actionLogText);
    addDockWidget(Qt::BottomDockWidgetArea, m_actionLogDock);

    m_mainTabWidget->addTab(m_homePage, QIcon(":/icons/dashboard.svg"), "Home");

    // ========================
    // Tab 1: Bulk Checksum
    // ========================
    m_bulkChecksumDock = new BulkChecksumDock(this);
    m_mainTabWidget->addTab(m_bulkChecksumDock, QIcon(":/icons/hash.svg"), "Bulk Checksum");

    // ========================
    // Tab 2: VDD Document Reviewer
    // ========================
    m_vddDocumentReviewer = new VddDocumentReviewer(this);
    m_mainTabWidget->addTab(m_vddDocumentReviewer, QIcon(":/icons/AI.svg"), "Document Review");
}

void MainWindow::logMessage(const QString& msg, const QString& type) {
    if (!m_actionLogText) return;
    
    QString color = "#94A3B8"; // default gray
    QString prefix = "[INFO]";
    
    if (type == "success") {
        color = "#22C55E"; // green
        prefix = "[SUCCESS]";
    } else if (type == "warning") {
        color = "#F59E0B"; // amber
        prefix = "[WARNING]";
    } else if (type == "error") {
        color = "#EF4444"; // red
        prefix = "[ERROR]";
    }
    
    QString html = QString("<span style='color: %1; font-weight: bold;'>%2</span> <span style='color: #F8FAFC;'>%3</span><br/>")
                   .arg(color).arg(prefix).arg(msg);
                   
    m_actionLogText->append(html);
}

void MainWindow::loadStyleSheet() {
    QFile file(":/styles/stylesheet.qss");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(QString::fromUtf8(file.readAll()));
        file.close();
    }
}

void MainWindow::onImportVddClicked() {
    QString odtPath = QFileDialog::getOpenFileName(this, "Select Version Description ODT Document", "", "OpenDocument Text (*.odt)");
    if (odtPath.isEmpty()) {
        return;
    }

    if (m_loadingDialog) m_loadingDialog->close();
    m_loadingDialog = new QProgressDialog(this);
    m_loadingDialog->setWindowTitle("Importing VDD Document");
    m_loadingDialog->setLabelText("Extracting and sending to AI LLM for analysis...");
    m_loadingDialog->setCancelButtonText(nullptr);
    m_loadingDialog->setRange(0, 0);
    m_loadingDialog->setMinimumDuration(0);
    m_loadingDialog->show();
    qApp->processEvents();

    m_statusLabel->setText(QString::asprintf("Extracting VDD Archive: '%s'...", QFileInfo(odtPath).fileName().toLocal8Bit().constData()));
    qApp->processEvents();

    if (!OdtExtractor::isValidOdt(odtPath)) {
        if (m_loadingDialog) m_loadingDialog->close();
        QMessageBox::critical(this, "VDD Parser Error", "Cannot read OpenDocument container. Archive might be corrupted.");
        m_statusLabel->setText("Parsing aborted: Invalid .odt file.");
        return;
    }

    QString plain = OdtExtractor::extractPlainText(odtPath);
    if (plain.isEmpty()) {
        if (m_loadingDialog) m_loadingDialog->close();
        QMessageBox::critical(this, "VDD Parser Error", "'content.xml' is missing from archive.");
        m_statusLabel->setText("Parsing aborted: missing content.xml.");
        return;
    }

    // Send to LLM for structured extraction
    m_records.clear();
    m_llmImportTargetId = 1;
    m_completedHashJobs = 0;
    m_activeHashJobs = 0;
    m_tableWidget->setRowCount(0);

    QString systemPrompt = "You are a precise data extraction assistant. You will receive extracted text from a VDD (Version Description Document) file. The document contains tables listing configuration items (CIs) with their references, versions, and checksums.\n\n"
        "CRITICAL INSTRUCTIONS:\n"
        "1. EXTRACT EVERY SINGLE CI ITEM you can find in the tables. Count them carefully. Do NOT skip any, even if they do not have a checksum or if they have non-standard checksum values.\n"
        "2. Each CI row has: File Name, CI Reference, Version, and optional checksum values.\n"
        "3. Distinguish MD5 vs CRC-32 by length:\n"
        "   - MD5 = exactly 32 hex characters (0-9, A-F) e.g., ABCDEF1234567890ABCDEF1234567890\n"
        "   - CRC-32 = exactly 8 hex characters e.g., 12345678\n"
        "   - 8-char values go in expectedCrc32, NOT expectedMd5\n"
        "4. For each CI, output: fileName (with extension like .exe/.zip/.7z if available, or just the file name), ciReference, version, expectedMd5 (32 hex or empty), expectedCrc32 (8 hex or empty)\n"
        "5. If a CI has only a CRC-32 (8-char) and no MD5, set expectedMd5 to empty string and expectedCrc32 to the 8-char value. If a CI has no checksum or a non-standard checksum, set both expectedMd5 and expectedCrc32 to empty strings.\n"
        "6. Count the total number of CI items BEFORE outputting. If the document has 20 CIs, your JSON array must have exactly 20 elements.\n\n"
        "Return ONLY a valid JSON array. No markdown, no explanation, no code fences, no backticks.\n\n"
        "Example output (plain text, no backticks):\n"
        "[{\"fileName\":\"DP-CRF-xxx.exe\",\"ciReference\":\"DP-CRF-xxx\",\"version\":\"1V00\",\"expectedMd5\":\"ABCDEF1234567890ABCDEF1234567890\",\"expectedCrc32\":\"12345678\"}]\n\n"
        "If you find no CI items, return an empty array: []";

    m_llmParseMode = "vdd";
    AIConfigManager::getInstance()->queryText(plain.trimmed(), systemPrompt, 9000);

    // Update Document Reviewer tab with current data
    if (m_vddDocumentReviewer) {
        m_vddDocumentReviewer->setOdtFilePath(odtPath);
        m_vddDocumentReviewer->setExistingRecords(m_records);
    }
}

void MainWindow::updateTableDisplay() {
    if (m_records.isEmpty()) {
        m_tableStack->setCurrentIndex(0);
        m_tableWidget->setRowCount(0);
        m_mainRowToDetailRow.clear();
        m_rowExpanded.clear();
        if (m_statisticsWidget) {
            m_statisticsWidget->updateStats(m_records);
        }
        return;
    } else {
        m_tableStack->setCurrentIndex(1);
    }

    // Remove all detail rows by iterating from bottom to top.
    // Detail rows are the values() of m_mainRowToDetailRow map.
    // Collect indices first, sort descending, then remove bottom-up
    // to avoid index-shifting issues.
    QList<int> detailIndices;
    for (int detailRow : m_mainRowToDetailRow.values()) {
        detailIndices.append(detailRow);
    }
    std::sort(detailIndices.begin(), detailIndices.end(), std::greater<int>());
    for (int dr : detailIndices) {
        m_tableWidget->removeRow(dr);
    }
    m_mainRowToDetailRow.clear();
    m_rowExpanded.clear();

    // Now clear all main rows
    m_tableWidget->setRowCount(0);
    int rowIdx = 0;

    // Separate VDD record IDs and CONFIG_ONLY record IDs — display VDD first, then CONFIG_ONLY at the end
    QList<int> vddIds, configOnlyIds;
    for (int id : m_records.keys()) {
        if (m_records[id].source == "CONFIG_ONLY")
            configOnlyIds.append(id);
        else
            vddIds.append(id);
    }

    // Process all records (VDD first, then CONFIG_ONLY)
    QList<int> allIds;
    allIds.reserve(vddIds.size() + configOnlyIds.size());
    allIds.append(vddIds);
    allIds.append(configOnlyIds);

    for (int id : allIds) {
        const VddRecord& rec = m_records[id];
        bool isConfigOnly = (rec.source == "CONFIG_ONLY");
        m_tableWidget->insertRow(rowIdx);

        QColor passColor(16, 185, 129);   // green
        QColor failColor(239, 68, 68);    // red
        QColor skipColor(107, 114, 128);  // grey

        // === COL 0: File Name (VDD) ===
        if (isConfigOnly) {
            m_tableWidget->setItem(rowIdx, 0, new QTableWidgetItem(rec.configFileName.isEmpty() ? "N/A" : rec.configFileName));
        } else {
            m_tableWidget->setItem(rowIdx, 0, new QTableWidgetItem(rec.fileName));
        }

        // === COL 1: File Name (Local Dir) ===
        m_tableWidget->setItem(rowIdx, 1, new QTableWidgetItem(rec.localFileName.isEmpty() ? "N/A" : rec.localFileName));

        // === COL 2: File Name Result ===
        QString fileNameResult = "N/A";
        if (rec.localStatus == "MATCH" || rec.localStatus == "MISMATCH") fileNameResult = "FOUND";
        else if (rec.localStatus == "MISSING") fileNameResult = "NOT FOUND";
        else if (rec.localStatus == "ERROR") fileNameResult = "ERROR";
        else if (rec.localStatus == "CONFIG_ONLY") fileNameResult = rec.configPathExists && rec.configFileFoundAtPath ? "FOUND" : "NOT FOUND";
        QTableWidgetItem* fileNameResultItem = new QTableWidgetItem(fileNameResult);
        if (fileNameResult == "FOUND") fileNameResultItem->setForeground(passColor);
        else if (fileNameResult == "NOT FOUND" || fileNameResult == "ERROR") fileNameResultItem->setForeground(failColor);
        else fileNameResultItem->setForeground(skipColor);
        m_tableWidget->setItem(rowIdx, 2, fileNameResultItem);

        // === COL 3: CI Ref (VDD) — N/A for CONFIG_ONLY (no VDD to compare) ===
        if (isConfigOnly) {
            m_tableWidget->setItem(rowIdx, 3, new QTableWidgetItem("N/A"));
        } else {
            m_tableWidget->setItem(rowIdx, 3, new QTableWidgetItem(rec.ciReference));
        }

        // === COL 4: CI Ref (Local Dir) ===
        m_tableWidget->setItem(rowIdx, 4, new QTableWidgetItem(rec.localCiRef.isEmpty() ? "N/A" : rec.localCiRef));

        // === COL 5: CI Ref (Config List) ===
        m_tableWidget->setItem(rowIdx, 5, new QTableWidgetItem(rec.configFileName.isEmpty() ? "N/A" : rec.configFileName));

        // === COL 6: CI Ref Result — SKIP for CONFIG_ONLY (no VDD CI ref to compare) ===
        if (isConfigOnly) {
            QTableWidgetItem* ciRefResultItem = new QTableWidgetItem("SKIP");
            ciRefResultItem->setForeground(skipColor);
            m_tableWidget->setItem(rowIdx, 6, ciRefResultItem);
        } else {
            QTableWidgetItem* ciRefResultItem = new QTableWidgetItem(rec.ciRefCheck == "PASS" ? "MATCH" : (rec.ciRefCheck == "MISMATCH" ? "NOT MATCH" : "SKIP"));
            ciRefResultItem->setForeground(rec.ciRefCheck == "PASS" ? passColor : (rec.ciRefCheck == "MISMATCH" ? failColor : skipColor));
            ciRefResultItem->setToolTip(rec.ciRefCheck == "MISMATCH" ? rec.fileStatusReason : "");
            m_tableWidget->setItem(rowIdx, 6, ciRefResultItem);
        }

        // === COL 7: Version (VDD) — CONFIG_ONLY uses config version, no VDD version to compare ===
        if (isConfigOnly) {
            m_tableWidget->setItem(rowIdx, 7, new QTableWidgetItem(rec.configVersion.isEmpty() ? "N/A" : rec.configVersion));
        } else {
            m_tableWidget->setItem(rowIdx, 7, new QTableWidgetItem(rec.version));
        }

        // === COL 8: Version (Local Dir) ===
        m_tableWidget->setItem(rowIdx, 8, new QTableWidgetItem(rec.localVersion.isEmpty() ? "N/A" : rec.localVersion));

        // === COL 9: Version (Config List) ===
        m_tableWidget->setItem(rowIdx, 9, new QTableWidgetItem(rec.configVersion.isEmpty() ? "N/A" : rec.configVersion));

        // === COL 10: Version Result — SKIP for CONFIG_ONLY (no VDD version to compare) ===
        if (isConfigOnly) {
            QTableWidgetItem* verResultItem = new QTableWidgetItem("SKIP");
            verResultItem->setForeground(skipColor);
            m_tableWidget->setItem(rowIdx, 10, verResultItem);
        } else {
            QString versionResultStr = rec.versionCheck == "PASS" ? "MATCH" : (rec.versionCheck == "MISMATCH" ? "NOT MATCH" : "SKIP");
            QTableWidgetItem* verResultItem = new QTableWidgetItem(versionResultStr);
            verResultItem->setForeground(rec.versionCheck == "PASS" ? passColor : (rec.versionCheck == "MISMATCH" ? failColor : skipColor));
            verResultItem->setToolTip(rec.versionCheck == "MISMATCH" ? rec.fileStatusReason : "");
            m_tableWidget->setItem(rowIdx, 10, verResultItem);
        }

        // === COL 11: MD5 (VDD) ===
        m_tableWidget->setItem(rowIdx, 11, new QTableWidgetItem(rec.expectedMd5));

        // === COL 12: MD5 (Local Dir) ===
        QString localMd5;
        if (rec.source == "CONFIG_ONLY") {
            localMd5 = rec.calculatedMd5.isEmpty() ? "Not calculated" : rec.calculatedMd5 + " (verified)";
        } else {
            localMd5 = rec.calculatedMd5.isEmpty() ? "Not calculated" : rec.calculatedMd5;
        }
        m_tableWidget->setItem(rowIdx, 12, new QTableWidgetItem(localMd5));

        // === COL 13: MD5 Result ===
        QString md5ResultStr;
        if (rec.source == "CONFIG_ONLY") {
            if (!rec.calculatedMd5.isEmpty()) md5ResultStr = "VERIFIED";
            else md5ResultStr = "NOT CALCULATED";
        } else {
            auto cleanHash = [](QString hash) -> QString {
                hash = hash.trimmed().toLower();
                if (hash.startsWith("0x")) {
                    hash = hash.mid(2);
                }
                return hash;
            };
            if (rec.calculatedMd5.isEmpty()) {
                md5ResultStr = "N/A";
            } else if (rec.expectedMd5.trimmed().isEmpty()) {
                md5ResultStr = "SKIP";
            } else if (cleanHash(rec.expectedMd5) == cleanHash(rec.calculatedMd5)) {
                md5ResultStr = "MATCH";
            } else {
                md5ResultStr = "NOT MATCH";
            }
        }
        QTableWidgetItem* md5ResultItem = new QTableWidgetItem(md5ResultStr);
        if (md5ResultStr == "MATCH" || md5ResultStr == "VERIFIED") md5ResultItem->setForeground(passColor);
        else if (md5ResultStr == "NOT MATCH") md5ResultItem->setForeground(failColor);
        else md5ResultItem->setForeground(skipColor);
        m_tableWidget->setItem(rowIdx, 13, md5ResultItem);

        // === COL 14: CRC-32 (VDD) ===
        m_tableWidget->setItem(rowIdx, 14, new QTableWidgetItem(rec.expectedCrc32.isEmpty() ? "-" : rec.expectedCrc32));

        // === COL 15: CRC-32 (Local Dir) ===
        QString localCrc32;
        if (rec.source == "CONFIG_ONLY") {
            localCrc32 = rec.calculatedCrc32.isEmpty() ? "-" : rec.calculatedCrc32 + " (verified)";
        } else {
            localCrc32 = rec.calculatedCrc32.isEmpty() ? "-" : rec.calculatedCrc32;
        }
        m_tableWidget->setItem(rowIdx, 15, new QTableWidgetItem(localCrc32));

        // === COL 16: CRC-32 Result ===
        QString crc32ResultStr;
        if (rec.source == "CONFIG_ONLY") {
            if (!rec.calculatedCrc32.isEmpty()) crc32ResultStr = "CALCULATED";
            else crc32ResultStr = "N/A";
        } else {
            auto cleanHash = [](QString hash) -> QString {
                hash = hash.trimmed().toLower();
                if (hash.startsWith("0x")) {
                    hash = hash.mid(2);
                }
                return hash;
            };
            if (rec.expectedCrc32.isEmpty() || rec.calculatedCrc32.isEmpty()) {
                crc32ResultStr = "SKIP";
            } else if (cleanHash(rec.expectedCrc32) == cleanHash(rec.calculatedCrc32)) {
                crc32ResultStr = "MATCH";
            } else {
                crc32ResultStr = "NOT MATCH";
            }
        }
        QTableWidgetItem* crc32ResultItem = new QTableWidgetItem(crc32ResultStr);
        if (crc32ResultStr == "MATCH" || crc32ResultStr == "CALCULATED") crc32ResultItem->setForeground(passColor);
        else if (crc32ResultStr == "NOT MATCH") crc32ResultItem->setForeground(failColor);
        else crc32ResultItem->setForeground(skipColor);
        m_tableWidget->setItem(rowIdx, 16, crc32ResultItem);

        // === COL 17: Document Link ===
        m_tableWidget->setItem(rowIdx, 17, new QTableWidgetItem(rec.configPath.isEmpty() ? "N/A" : rec.configPath));

        // === COL 18: Doc Link Result ===
        QTableWidgetItem* docLinkResultItem = new QTableWidgetItem(rec.pathCheck == "PASS" ? "MATCH" : (rec.pathCheck == "MISMATCH" ? "NOT MATCH" : "SKIP"));
        docLinkResultItem->setForeground(rec.pathCheck == "PASS" ? passColor : (rec.pathCheck == "MISMATCH" ? failColor : skipColor));
        docLinkResultItem->setToolTip(rec.pathCheck == "MISMATCH" ? rec.fileStatusReason : "");
        m_tableWidget->setItem(rowIdx, 18, docLinkResultItem);

        // === COL 19: Source ===
        QString sourceStr = (rec.source == "CONFIG_ONLY") ? "Config-Only" : "VDD";
        QTableWidgetItem* sourceItem = new QTableWidgetItem(sourceStr);
        if (rec.source == "CONFIG_ONLY") {
            sourceItem->setForeground(QColor(245, 158, 11));
        }
        m_tableWidget->setItem(rowIdx, 19, sourceItem);

        setRowStatusColors(rowIdx, rec.localStatus, rec.fileStatus);
        m_rowExpanded[rowIdx] = false;

        rowIdx++;
    }

    if (m_statisticsWidget) {
        m_statisticsWidget->updateStats(m_records);
    }
}

void MainWindow::setRowStatusColors(int row, const QString& localStatus, const QString& fileStatus) {
    if (localStatus == "MATCH" && fileStatus == "MATCH") {
        for (int i = 0; i < m_tableWidget->columnCount(); ++i) {
            if (m_tableWidget->item(row, i))
                m_tableWidget->item(row, i)->setBackground(QColor(16, 185, 129, 20));
        }
    } else if (localStatus == "CONFIG_ONLY") {
        for (int i = 0; i < m_tableWidget->columnCount(); ++i) {
            if (m_tableWidget->item(row, i))
                m_tableWidget->item(row, i)->setBackground(QColor(245, 158, 11, 20));
        }
    } else if (localStatus == "MISMATCH" || fileStatus == "NOT_IN_FILE") {
        for (int i = 0; i < m_tableWidget->columnCount(); ++i) {
            if (m_tableWidget->item(row, i))
                m_tableWidget->item(row, i)->setBackground(QColor(239, 68, 68, 15));
        }
    }
}

void MainWindow::toggleRowDetails(int row) {
    // Check if row has hash details (only expand if Calculated MD5 is available)
    if (row < 0 || row >= m_tableWidget->rowCount()) {
        return;
    }

    QTableWidgetItem* md5Item = m_tableWidget->item(row, 12);
    if (!md5Item) return;
    if (md5Item->text() == "Not calculated" || md5Item->text() == "VERIFIED" || md5Item->text() == "NOT CALCULATED" || md5Item->text() == "CALCULATED") {
        // Allow expand for CONFIG_ONLY rows with computed hashes only
        if (md5Item->text() == "Not calculated") return;
        // For VERIFIED/NOT CALCULATED, check source
        QTableWidgetItem* fileNameItem2 = m_tableWidget->item(row, 0);
        if (!fileNameItem2) return;
        for (int id : m_records.keys()) {
            const VddRecord& chk = m_records[id];
            QString col0 = chk.source == "CONFIG_ONLY" ? chk.configFileName : chk.fileName;
            if (col0 == fileNameItem2->text() && chk.source != "CONFIG_ONLY") {
                return;
            }
        }
    }

    QTableWidgetItem* fileNameItem = m_tableWidget->item(row, 0);
    if (!fileNameItem) return;

    // Find the record matching this row — check configFileName for CONFIG_ONLY rows
    int recordId = -1;
    for (int id : m_records.keys()) {
        const VddRecord& chk = m_records[id];
        QString col0 = chk.source == "CONFIG_ONLY" ? chk.configFileName : chk.fileName;
        if (col0 == fileNameItem->text()) {
            recordId = id;
            break;
        }
    }
    if (recordId < 0 || !m_records.contains(recordId)) return;

    // Toggle expansion state
    bool currentlyExpanded = m_rowExpanded.value(row, false);

    if (currentlyExpanded) {
        // Collapse: remove detail row
        if (m_mainRowToDetailRow.contains(row)) {
            int detailRow = m_mainRowToDetailRow[row];
            m_tableWidget->removeRow(detailRow);
            m_mainRowToDetailRow.remove(row);

            // Update row mappings for rows below
            QMap<int, int> newMappings;
            for (auto it = m_mainRowToDetailRow.begin(); it != m_mainRowToDetailRow.end(); ++it) {
                if (it.key() > row) {
                    newMappings[it.key() - 1] = it.value() - 1;
                } else {
                    newMappings[it.key()] = it.value();
                }
            }
            m_mainRowToDetailRow = newMappings;
        }

        m_rowExpanded[row] = false;
    } else {
        // Expand: create detail row
        const VddRecord& rec = m_records[recordId];
        createDetailRow(row, rec);
    }
}

void MainWindow::createDetailRow(int mainRow, const VddRecord& record) {
    // Insert new row after the main row
    int detailRow = mainRow + 1;
    m_tableWidget->insertRow(detailRow);

    // Create detail widget with hash information
    QWidget* detailWidget = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(detailWidget);
    layout->setContentsMargins(20, 8, 20, 8);
    layout->setSpacing(20);

    // Add hash details with styled labels
    QVBoxLayout* hashLayout = new QVBoxLayout();

    // SHA-1
    QLabel* sha1Label = new QLabel("SHA-1:");
    sha1Label->setStyleSheet("color: #94A3B8; font-weight: 600; font-size: 11px;");
    QLabel* sha1Value = new QLabel(record.calculatedSha1.isEmpty() ? "Not calculated" : record.calculatedSha1);
    sha1Value->setStyleSheet("color: #F8FAFC; font-family: 'Consolas', 'Monaco', monospace; font-size: 11px;");
    sha1Value->setWordWrap(true);

    QHBoxLayout* sha1Row = new QHBoxLayout();
    sha1Row->addWidget(sha1Label);
    sha1Row->addWidget(sha1Value, 1);
    hashLayout->addLayout(sha1Row);

    // CRC-32
    QLabel* crcLabel = new QLabel("CRC-32:");
    crcLabel->setStyleSheet("color: #94A3B8; font-weight: 600; font-size: 11px;");
    QLabel* crcValue = new QLabel(record.calculatedCrc32.isEmpty() ? "Not calculated" : record.calculatedCrc32);
    crcValue->setStyleSheet("color: #F8FAFC; font-family: 'Consolas', 'Monaco', monospace; font-size: 11px;");

    QHBoxLayout* crcRow = new QHBoxLayout();
    crcRow->addWidget(crcLabel);
    crcRow->addWidget(crcValue, 1);
    hashLayout->addLayout(crcRow);

    layout->addLayout(hashLayout);
    layout->addStretch();

    // Add Path Audit Details if a path is specified in configuration file
    if (!record.configPath.isEmpty()) {
        QVBoxLayout* pathLayout = new QVBoxLayout();
        
        QLabel* pathTitle = new QLabel("Configuration Link Audit:", this);
        pathTitle->setStyleSheet("color: #6366F1; font-weight: 700; font-size: 11px; margin-top: 4px;");
        pathLayout->addWidget(pathTitle);

        QLabel* pathLabel = new QLabel(QString("Link Location: %1").arg(record.configPath), this);
        pathLabel->setStyleSheet("color: #E2E8F0; font-family: 'Consolas', 'Monaco', monospace; font-size: 11px;");
        pathLabel->setWordWrap(true);
        pathLayout->addWidget(pathLabel);

        QLabel* statusLabel = new QLabel(this);
        if (record.configPathExists && record.configFileFoundAtPath) {
            statusLabel->setText("✓ Path Valid & File Found at Location");
            statusLabel->setStyleSheet("color: #22C55E; font-weight: 600; font-size: 11px;");
        } else if (record.configPathExists) {
            statusLabel->setText("⚠ Path Valid but File Missing from Location");
            statusLabel->setStyleSheet("color: #F59E0B; font-weight: 600; font-size: 11px;");
        } else {
            statusLabel->setText("✗ Path Specified is Invalid / Not Accessible");
            statusLabel->setStyleSheet("color: #EF4444; font-weight: 600; font-size: 11px;");
        }
        pathLayout->addWidget(statusLabel);
        
        layout->addLayout(pathLayout, 1);
    }

    // Add comprehensive verdict audit details
    QVBoxLayout* verdictLayout = new QVBoxLayout();
    QLabel* verdictTitle = new QLabel("Audit Verdict Details:", this);
    verdictTitle->setStyleSheet("color: #EC4899; font-weight: 700; font-size: 11px; margin-top: 4px;");
    verdictLayout->addWidget(verdictTitle);

    QString localReason = record.localStatusReason.isEmpty() ? "Not calculated or verified yet." : record.localStatusReason;
    QLabel* localReasonLabel = new QLabel(QString("Local match: %1").arg(localReason), this);
    localReasonLabel->setStyleSheet("color: #CBD5E1; font-size: 11px;");
    localReasonLabel->setWordWrap(true);
    verdictLayout->addWidget(localReasonLabel);

    QString fileReason = record.fileStatusReason.isEmpty() ? "Not verified against configuration lists yet." : record.fileStatusReason;
    QLabel* fileReasonLabel = new QLabel(QString("Config match: %1").arg(fileReason), this);
    fileReasonLabel->setStyleSheet("color: #CBD5E1; font-size: 11px;");
    fileReasonLabel->setWordWrap(true);
    verdictLayout->addWidget(fileReasonLabel);

    layout->addLayout(verdictLayout, 2);

    // Apply detail row styling
    detailWidget->setStyleSheet(
        "QWidget { background: rgba(99, 102, 241, 0.05); border-radius: 6px; }"
    );

    // Merge cells for the entire detail row
    m_tableWidget->setSpan(detailRow, 0, 1, m_tableWidget->columnCount());
    m_tableWidget->setCellWidget(detailRow, 0, detailWidget);

    // Store mapping
    m_mainRowToDetailRow[mainRow] = detailRow;

    // Update row mappings for rows below
    QMap<int, int> newMappings;
    for (auto it = m_mainRowToDetailRow.begin(); it != m_mainRowToDetailRow.end(); ++it) {
        if (it.key() > mainRow) {
            newMappings[it.key() + 1] = it.value() + 1;
        } else {
            newMappings[it.key()] = it.value();
        }
    }
    m_mainRowToDetailRow = newMappings;

    // Update expand icon
    QLabel* expandIcon = qobject_cast<QLabel*>(m_tableWidget->cellWidget(mainRow, 0));
    if (expandIcon) {
        expandIcon->setText("▼"); // Down arrow
    }

    m_rowExpanded[mainRow] = true;
}

void MainWindow::onVerifyLocalClicked() {
    if (m_csvFilePaths.isEmpty()) {
        QMessageBox::warning(this, "No Configuration File", "Load a Configuration Items file (CSV/XLSX) first. Click 'Browse...' under 'Configuration Items List' to select the file, then click 'Verify All'.");
        return;
    }

    // Phase 1: Parse config file(s)
    m_statusLabel->setText("Parsing configuration file(s)...");
    qApp->processEvents();

    QList<QMap<QString, QString>> configItems = parseConfigFileItems(m_csvFilePaths);
    if (configItems.isEmpty()) {
        QMessageBox::critical(this, "Config Parse Error", "Could not parse any configuration items from the selected file(s).");
        return;
    }

    // Phase 2: Enhance parsed items with path verification
    m_statusLabel->setText("Verifying configuration paths...");
    qApp->processEvents();

    for (auto& item : configItems) {
        QString docLink = item.value("documentLink", "").trimmed();
        bool pathExists = false;
        bool fileFound = false;
        QString ciRef = item.value("fileName", "");
        QString docLinkClean = docLink.trimmed();

        if (!docLinkClean.isEmpty()) {
            QFileInfo directFi(docLinkClean);
            bool isNetworkPath = docLinkClean.startsWith("\\\\");

            if (directFi.isFile() && directFi.exists()) {
                pathExists = true;
                fileFound = true;
            } else if (directFi.isDir() || (!directFi.exists() && QDir(docLinkClean).exists())) {
                pathExists = true;
                QStringList nameFilters;
                QFileInfo ciFi(ciRef);
                QString ciBase = ciFi.baseName();
                QString ciSuffix = ciFi.suffix();
                QDir searchDirObj(docLinkClean);
                nameFilters << ciRef << ciFi.fileName() << ciBase + "." + ciSuffix << ciBase << ".*";
                // Deduplicate
                { QSet<QString> seen; QStringList deduped; for (const QString& n : nameFilters) { if (!seen.contains(n)) { seen.insert(n); deduped.append(n); } } nameFilters = deduped; }
                QFileInfoList fileList = searchDirObj.entryInfoList(nameFilters, QDir::Files);
                if (!fileList.isEmpty()) fileFound = true;
                if (!fileFound) {
                    QDirIterator subIt(docLinkClean, QDirIterator::Subdirectories);
                    while (subIt.hasNext()) {
                        QFileInfo subFi(subIt.next());
                        QString subBase = subFi.baseName();
                        subBase.remove(".-_");
                        if (subBase == ciBase || subFi.fileName().toLower() == ciRef.toLower() || subFi.fileName().toLower().contains(ciRef.toLower())) {
                            fileFound = true;
                            break;
                        }
                    }
                }
            } else if (isNetworkPath) {
                pathExists = true;
                fileFound = false;
            }
        }

        item["pathExists"] = pathExists ? "true" : "false";
        item["fileFoundAtPath"] = fileFound ? "true" : "false";
    }

    // Phase 3: Run CI metadata verification (version, MD5, CI ref, component, path)
    m_statusLabel->setText("Verifying CI metadata against configuration...");
    qApp->processEvents();

    verifyAgainstParsedItems(configItems);

    // Phase 2.5: Detect config-only items (present in config but NOT in VDD)
    m_statusLabel->setText("Detecting configuration-only items...");
    qApp->processEvents();

    // Remove stale CONFIG_ONLY records from any previous run before creating new ones.
    // Without this, every "Verify All" call appends new CONFIG_ONLY rows on top of old ones.
    {
        QList<int> staleIds;
        for (int id : m_records.keys()) {
            if (m_records[id].source == "CONFIG_ONLY")
                staleIds.append(id);
        }
        for (int id : staleIds)
            m_records.remove(id);
    }

    QSet<QString> matchedConfigFileNames;
    for (const VddRecord& rec : m_records.values()) {
        if (!rec.configFileName.isEmpty()) {
            QString fn = rec.configFileName.toLower().trimmed();
            matchedConfigFileNames.insert(fn);
            QString clean = fn;
            clean.remove(' ');
            if (!clean.isEmpty()) matchedConfigFileNames.insert(clean);
        }
    }

    int initialRecordsCount = m_records.size();
    int configOnlyCount = 0;
    QList<VddRecord> duplicatesToAdd;

    for (const auto& item : configItems) {
        QString ciKey = item.value("fileName", "").trimmed().toLower();
        if (ciKey.isEmpty()) continue;

        bool isMatched = false;
        int matchedVddId = -1;

        // 1) Check if this config item was already associated with a VDD record.
        if (matchedConfigFileNames.contains(ciKey)) {
            continue;
        }

        // 2) Check other match conditions against all VDD records (fileName, ciReference, baseName, localCiRef)
        if (!isMatched) {
            for (int id : m_records.keys()) {
                if (m_records[id].source == "CONFIG_ONLY") continue;
                const VddRecord& rec = m_records[id];

                // 2a) fileName match (exact)
                QString recFn = rec.fileName.toLower().trimmed();
                if (ciKey == recFn) { isMatched = true; matchedVddId = id; break; }
                QString ciKeyClean = ciKey; ciKeyClean.remove(' ');
                QString recFnClean = recFn; recFnClean.remove(' ');
                if (!ciKeyClean.isEmpty() && ciKeyClean == recFnClean) { isMatched = true; matchedVddId = id; break; }

                // 2b) baseName match (exact)
                QString ciBase = QFileInfo(ciKey).baseName().toLower();
                QString recFnBase = QFileInfo(recFn).baseName().toLower();
                if (ciBase == recFnBase) { isMatched = true; matchedVddId = id; break; }
                QString ciBaseClean = ciBase; ciBaseClean.remove(' ');
                QString recFnBaseClean = recFnBase; recFnBaseClean.remove(' ');
                if (!ciBaseClean.isEmpty() && ciBaseClean == recFnBaseClean) { isMatched = true; matchedVddId = id; break; }

                // 2c) ciReference match (exact)
                QString recCiRef = rec.ciReference.toLower().trimmed();
                if (ciKey == recCiRef) { isMatched = true; matchedVddId = id; break; }
                QString recCiRefClean = recCiRef; recCiRefClean.remove(' ');
                if (!ciKeyClean.isEmpty() && ciKeyClean == recCiRefClean) { isMatched = true; matchedVddId = id; break; }
                if (ciBase == recCiRef) { isMatched = true; matchedVddId = id; break; }
                if (ciBaseClean == recCiRefClean) { isMatched = true; matchedVddId = id; break; }

                // 2d) localCiRef baseName match
                QString recLocalCiRef = rec.localCiRef.toLower().trimmed();
                if (!recLocalCiRef.isEmpty()) {
                    if (ciKey == recLocalCiRef) { isMatched = true; matchedVddId = id; break; }
                    if (ciKeyClean == recLocalCiRef) { isMatched = true; matchedVddId = id; break; }
                    if (ciBase == recLocalCiRef) { isMatched = true; matchedVddId = id; break; }
                    QString recLocalCiBase = QFileInfo(recLocalCiRef).baseName().toLower();
                    if (ciBase == recLocalCiBase) { isMatched = true; matchedVddId = id; break; }
                    if (ciBaseClean == recLocalCiBase) { isMatched = true; matchedVddId = id; break; }
                }

                // 2e) Fuzzy containment
                if ((ciKey.length() >= 12 && recFn.length() >= 12 && (ciKey.contains(recFn) || recFn.contains(ciKey))) ||
                    (ciBase.length() >= 12 && recFnBase.length() >= 12 && (ciBase.contains(recFnBase) || recFnBase.contains(ciBase))) ||
                    (ciKey.length() >= 12 && recCiRef.length() >= 12 && (ciKey.contains(recCiRef) || recCiRef.contains(ciKey))) ||
                    (ciBase.length() >= 12 && recCiRef.length() >= 12 && (ciBase.contains(recCiRef) || recCiRef.contains(ciBase)))) {
                    isMatched = true; matchedVddId = id; break;
                }
            }
        }

        // 3) Try suffix-stripped match as final VDD lookup fallback
        if (!isMatched) {
            QString ciStripped = stripVersionSuffix(ciKey);
            for (int id : m_records.keys()) {
                if (m_records[id].source == "CONFIG_ONLY") continue;
                QString recFnStripped = stripVersionSuffix(m_records[id].fileName);
                QString recCiStripped = stripVersionSuffix(m_records[id].ciReference);
                if (ciStripped == recFnStripped || ciStripped == recCiStripped) {
                    isMatched = true;
                    matchedVddId = id;
                    break;
                }
            }
        }

        if (isMatched && matchedVddId != -1) {
            // This config item matched a VDD record - update the VDD record with config metadata.
            // If the same CI appeared again later in the config list, it would have been
            // caught by the alreadyAssociated check above and skipped.
            VddRecord& origRec = m_records[matchedVddId];
            if (origRec.configFileName.isEmpty()) {
                origRec.configFileName = item.value("fileName", "").trimmed();
                origRec.configVersion = item.value("version", "").trimmed();
                origRec.configPath = item.value("documentLink", "").trimmed();
                origRec.configComponent = item.value("component", "").trimmed();
                origRec.configPathExists = item.value("pathExists", "false") == "true";
                origRec.configFileFoundAtPath = item.value("fileFoundAtPath", "false") == "true";

                if (ciKey.contains("srs") || ciKey.contains("sdd") || ciKey.contains("vdd") || ciKey.contains("stid")) {
                    logMessage(QString("MATCH found for config item: %1").arg(ciKey), "info");
                }
            }
            // else: the VDD record already has config metadata from an earlier config row.
            // The alreadyAssociated check above would have caught subsequent duplicates.
            continue;
        }


        VddRecord configOnlyRec;
        configOnlyRec.id = -(initialRecordsCount + 1 + configOnlyCount);
        configOnlyRec.source = "CONFIG_ONLY";
        configOnlyRec.fileName = item.value("fileName", "");
        configOnlyRec.ciReference = item.value("fileName", "");
        configOnlyRec.configFileName = item.value("fileName", "");
        configOnlyRec.configVersion = item.value("version", "");
        configOnlyRec.configPath = item.value("documentLink", "");
        configOnlyRec.configComponent = item.value("component", "");
        configOnlyRec.configPathExists = item.value("pathExists", "false") == "true";
        configOnlyRec.configFileFoundAtPath = item.value("fileFoundAtPath", "false") == "true";
        configOnlyRec.localStatus = "CONFIG_ONLY";
        configOnlyRec.fileStatus = "CONFIG_ONLY";
        configOnlyRec.expectedMd5 = item.value("expectedMd5", "");
        configOnlyRec.localCiRef = QFileInfo(configOnlyRec.fileName).baseName();
        configOnlyRec.versionCheck = "SKIP";
        configOnlyRec.md5Check = "SKIP";
        configOnlyRec.crc32Check = "SKIP";

        QString docLinkClean = item.value("documentLink", "").trimmed();
        if (!docLinkClean.isEmpty()) {
            configOnlyRec.pathCheck = (configOnlyRec.configPathExists && configOnlyRec.configFileFoundAtPath) ? "PASS" : "MISMATCH";
        } else {
            configOnlyRec.pathCheck = "SKIP";
        }
        configOnlyRec.ciRefCheck = "SKIP";
        configOnlyRec.compCheck = "SKIP";
        configOnlyRec.version = item.value("version", ""); // Show config version in VDD column

        QString statusReason = QString("CI item '%1' is present in configuration but not found in the imported VDD document.").arg(configOnlyRec.fileName);
        if (configOnlyRec.configPath.isEmpty()) {
            statusReason = QString("CI item '%1' is present in configuration but not found in the imported VDD document (no Document Link provided).").arg(configOnlyRec.fileName);
        } else if (!configOnlyRec.configPathExists) {
            statusReason = QString("CI item '%1' is present in configuration but not found in the imported VDD document (Document Link path does not exist).").arg(configOnlyRec.fileName);
        } else if (!configOnlyRec.configFileFoundAtPath) {
            statusReason = QString("CI item '%1' is present in configuration but not found in the imported VDD document (file not found at Document Link path).").arg(configOnlyRec.fileName);
        }
        configOnlyRec.fileStatusReason = statusReason;

        m_records[configOnlyRec.id] = configOnlyRec;
        configOnlyCount++;
    }

    if (configOnlyCount > 0) {
        logMessage(QString("Found %1 configuration-only CI items (not present in VDD).").arg(configOnlyCount), "warning");
    } else {
        logMessage("All configuration items matched VDD records. No config-only items detected.", "info");
    }

    // Phase 4: Resolve file paths and compute checksums
    m_activeHashJobs = 0;
    m_completedHashJobs = 0;
    m_totalHashFiles = 0;
    m_fileProgressMap.clear();

    m_statusLabel->setText("Resolving file paths from configuration...");
    qApp->processEvents();

    // First pass: resolve all records, count total (hashable + MISSING), seed progress map
    for (auto recIt = m_records.begin(); recIt != m_records.end(); ++recIt) {
        VddRecord& rec = recIt.value();
        // CONFIG_ONLY records: resolve path from config document link and compute checksum
        if (rec.source == "CONFIG_ONLY") {
            QString fullPath = resolveFilePathForRecord(rec, configItems);
            if (!fullPath.isEmpty()) {
                rec.localFileName = QFileInfo(fullPath).fileName();
                rec.localCiRef = QFileInfo(fullPath).baseName();
                rec.localFullPath = fullPath;
                QString extractedVersion = extractVersionFromDocument(fullPath);
                if (!extractedVersion.isEmpty()) {
                    rec.localVersion = extractedVersion;
                }
                m_fileProgressMap[rec.id] = 0;
                m_totalHashFiles++;
            } else {
                rec.localStatus = "CONFIG_ONLY";
                rec.localFileName = "NOT FOUND";
                rec.localCiRef = "N/A";
                rec.localStatusReason = "CI item is in configuration but file not found at Document Link path.";
                m_fileProgressMap[rec.id] = 100;
                m_totalHashFiles++;
            }
            continue;
        }
        QString fullPath = resolveFilePathForRecord(rec, configItems);

        if (!fullPath.isEmpty()) {
            // Seed progress map for hashable files
            m_fileProgressMap[rec.id] = 0;
            m_totalHashFiles++;
        } else {
            // MISSING: seed progress map at 100 immediately
            m_fileProgressMap[rec.id] = 100;
            m_totalHashFiles++;
        }
    }
    if (m_totalHashFiles < 1) m_totalHashFiles = 1;

    // Second pass: launch hashing threads for files that exist
    for (auto recIt = m_records.begin(); recIt != m_records.end(); ++recIt) {
        VddRecord& rec = recIt.value();
        // CONFIG_ONLY: file path already resolved in first pass
        if (rec.source == "CONFIG_ONLY") {
            if (!rec.localFullPath.isEmpty()) {
                QString fullPath = rec.localFullPath;

                QThread* thread = new QThread(this);
                ChecksumWorker* worker = new ChecksumWorker(rec.id, fullPath);
                worker->moveToThread(thread);

                connect(thread, &QThread::started, worker, &ChecksumWorker::process);
                connect(worker, &ChecksumWorker::started, this, &MainWindow::onHashStarted, Qt::QueuedConnection);
                connect(worker, &ChecksumWorker::fileProgress, this, &MainWindow::onHashProgress, Qt::QueuedConnection);
                connect(worker, &ChecksumWorker::finished, this, &MainWindow::onHashFinished, Qt::QueuedConnection);
                connect(worker, &ChecksumWorker::finished, thread, &QThread::quit);
                connect(worker, &ChecksumWorker::finished, worker, &ChecksumWorker::deleteLater);
                connect(thread, &QThread::finished, thread, &QThread::deleteLater);

                m_verifierThreads.insert(rec.id, thread);
                thread->start();
            }
            continue;
        }
        // VDD records: resolve path and launch hashing
        QString fullPath = resolveFilePathForRecord(rec, configItems);

        if (!fullPath.isEmpty()) {
            VddRecord updated = m_records[rec.id];
            updated.localFileName = QFileInfo(fullPath).fileName();
            updated.localCiRef = QFileInfo(fullPath).baseName();
            updated.localFullPath = fullPath;
            QString extractedVersion = extractVersionFromDocument(fullPath);
            if (!extractedVersion.isEmpty()) {
                updated.localVersion = extractedVersion;
            }
            m_records[rec.id] = updated;

            QThread* thread = new QThread(this);
            ChecksumWorker* worker = new ChecksumWorker(rec.id, fullPath);
            worker->moveToThread(thread);

            connect(thread, &QThread::started, worker, &ChecksumWorker::process);
            connect(worker, &ChecksumWorker::started, this, &MainWindow::onHashStarted, Qt::QueuedConnection);
            connect(worker, &ChecksumWorker::fileProgress, this, &MainWindow::onHashProgress, Qt::QueuedConnection);
            connect(worker, &ChecksumWorker::finished, this, &MainWindow::onHashFinished, Qt::QueuedConnection);
            connect(worker, &ChecksumWorker::finished, thread, &QThread::quit);
            connect(worker, &ChecksumWorker::finished, worker, &ChecksumWorker::deleteLater);
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);

            m_verifierThreads.insert(rec.id, thread);
            thread->start();
        } else {
            VddRecord updated = m_records[rec.id];
            updated.localStatus = "MISSING";
            updated.localFileName = "NOT FOUND";
            updated.localCiRef = "N/A";
            updated.localStatusReason = QString("File not found in configuration Document Link paths.");
            m_records[rec.id] = updated;
            logMessage(QString("File '%1' NOT FOUND in config Document Link paths.").arg(rec.fileName), "warning");
        }
    }

    updateTableDisplay();
}

void MainWindow::onHashStarted(int recordId) {
    m_activeHashJobs++;
    if (!m_fileProgressMap.contains(recordId)) {
        m_fileProgressMap[recordId] = 0;
        m_totalHashFiles++;
    }
    m_statusLabel->setText(QString::asprintf("Computing checksums dynamically. Active background threads: %d...", m_activeHashJobs));
}

void MainWindow::onHashProgress(int recordId, int progress) {
    m_fileProgressMap[recordId] = progress;

    // Update specific row cell with hashing progress (col 12 = Calculated MD5)
    if (!m_records.contains(recordId)) return;
    const VddRecord& pr = m_records[recordId];
    QString targetFileName = (pr.source == "CONFIG_ONLY") ? pr.configFileName : pr.fileName;
    if (targetFileName.isEmpty()) return;

    for (int r = 0; r < m_tableWidget->rowCount(); ++r) {
        QTableWidgetItem* fnItem = m_tableWidget->item(r, 0);
        if (fnItem && fnItem->text() == targetFileName) {
            QTableWidgetItem* hashItem = m_tableWidget->item(r, 12);
            if (hashItem) {
                hashItem->setText(QString("Hashing: %1%").arg(progress));
            }
            break;
        }
    }
}

void MainWindow::updateProgressSlow() {
    if (m_totalHashFiles < 1) return;

    m_progressMutex.lock();
    double totalProgress = 0.0;
    for (int progress : m_fileProgressMap) {
        totalProgress += progress;
    }
    m_progressMutex.unlock();

    double overall = totalProgress / (m_totalHashFiles * 100.0) * 100.0;
    int clamped = qMin(100, qMax(0, static_cast<int>(overall)));
    m_localProgressBar->setValue(clamped);
}

void MainWindow::onHashFinished(int recordId, const QString& calculatedMd5, const QString& calculatedSha1, const QString& calculatedCrc32, bool success, const QString& errorStr) {
    m_activeHashJobs--;
    m_completedHashJobs++;

    if (!m_records.contains(recordId)) {
        return;
    }

    VddRecord rec = m_records[recordId];
    if (!success) {
        rec.localStatus = (rec.source == "CONFIG_ONLY") ? "CONFIG_ONLY" : "ERROR";
        rec.calculatedMd5 = errorStr;
        rec.calculatedSha1 = "";
        rec.calculatedCrc32 = "";
        rec.localStatusReason = QString("Hashing failed for file '%1'. Error: %2").arg(rec.fileName).arg(errorStr);
        logMessage(QString("Hashing ERROR for file '%1': %2").arg(rec.fileName).arg(errorStr), "error");
    } else {
        rec.calculatedMd5 = calculatedMd5;
        rec.calculatedSha1 = calculatedSha1;
        rec.calculatedCrc32 = calculatedCrc32;


        // For CONFIG_ONLY records, skip VDD-side checksum comparison
        if (rec.source == "CONFIG_ONLY") {
            rec.localStatus = "CONFIG_ONLY";
            rec.localStatusReason = QString("Physical file verified: calculated MD5 hash = %1 (config-only item, no VDD reference).").arg(calculatedMd5);
            logMessage(QString("Config-only file '%1': MD5 = %2").arg(rec.fileName).arg(calculatedMd5), "info");
        } else {
            auto cleanHash = [](QString hash) -> QString {
                hash = hash.trimmed().toLower();
                if (hash.startsWith("0x")) {
                    hash = hash.mid(2);
                }
                return hash;
            };

            QString expMd5 = cleanHash(rec.expectedMd5);
            QString expCrc = cleanHash(rec.expectedCrc32);
            QString calcMd5 = cleanHash(calculatedMd5);
            QString calcCrc = cleanHash(calculatedCrc32);

            bool hasExpectedMd5 = !expMd5.isEmpty();
            bool hasExpectedCrc = !expCrc.isEmpty();

            bool md5Matches = !hasExpectedMd5 || (expMd5 == calcMd5);
            bool crcMatches = !hasExpectedCrc || (expCrc == calcCrc);

            if (md5Matches && crcMatches) {
                rec.localStatus = "MATCH";
                if (hasExpectedMd5 && hasExpectedCrc) {
                    rec.localStatusReason = QString("Physical file verified: calculated MD5 and CRC-32 hashes match VDD expected checksums.");
                } else if (hasExpectedMd5) {
                    rec.localStatusReason = QString("Physical file verified: calculated MD5 hash matches VDD expected checksum.");
                } else if (hasExpectedCrc) {
                    rec.localStatusReason = QString("Physical file verified: calculated CRC-32 hash matches VDD expected checksum.");
                } else {
                    rec.localStatusReason = QString("Physical file verified (no integrity checksums specified in VDD).");
                }
                logMessage(QString("Verified file '%1': integrity checks passed.").arg(rec.fileName), "success");
            } else {
                rec.localStatus = "MISMATCH";
                QStringList errors;
                if (!md5Matches) {
                    errors.append(QString("Calculated MD5 '%1' does not match VDD expected '%2'").arg(calculatedMd5).arg(rec.expectedMd5));
                }
                if (!crcMatches) {
                    errors.append(QString("Calculated CRC-32 '%1' does not match VDD expected '%2'").arg(calculatedCrc32).arg(rec.expectedCrc32));
                }
                rec.localStatusReason = "Integrity violation: " + errors.join(" / ");
                logMessage(QString("Mismatch found in file '%1': %2.").arg(rec.fileName).arg(errors.join(", ")), "error");
            }
        }
    }

    m_records[recordId] = rec;
    m_verifierThreads.remove(recordId);

    updateTableDisplay();

    // Update statistics widget
    m_statisticsWidget->updateStats(m_records);

    // Mark this file as fully complete (100%) so it contributes its full share
    m_fileProgressMap[recordId] = 100;

    // Set progress: 100 when all done
    if (m_activeHashJobs <= 0) {
        m_statusLabel->setText("Multi-threaded checksum generation completed successfully for target release registry.");
        logMessage("Finished local hashes verification cycle.", "info");
    }
}

// Helper: parse config file and return list of CI items
QList<QMap<QString, QString>> MainWindow::parseConfigFileItems(const QStringList& filePaths) {
    QList<QMap<QString, QString>> parsedItems;

    for (const QString& filePath : filePaths) {
        QFileInfo fi(filePath);
        if (!fi.exists()) continue;

        QString ext = fi.suffix().toLower();
        QStringList rows;

        if (ext == "csv") {
            rows = parseCSV(filePath);
        } else {
            continue;
        }

        if (rows.isEmpty()) continue;

        // Detect delimiter on the raw first row (before HTML stripping)
        QChar delimiter = ',';
        {
            QString firstLine = rows[0];
            if (firstLine.count(';') > firstLine.count(',')) {
                delimiter = ';';
            }
        }

        // Strip HTML tags from the first row before parsing headers
        QString cleanFirstLine = rows[0];
        cleanFirstLine.replace(QRegExp("<[^>]+>"), " ");
        QStringList headers = parseCSVLine(cleanFirstLine, delimiter);
        for (int i = 0; i < headers.size(); i++) {
            headers[i] = headers[i].trimmed();
        }

        // Identify column positions by matching header names.
        // Multiple synonyms are checked so that headers like "CI Ref", "CI_Ref",
        // "Ref", or abbreviated variants are all recognised.
        int ciRefCol = -1, ciVerCol = -1, md5Col = -1, docLinkCol = -1, compCol = -1;
        for (int i = 0; i < headers.size(); i++) {
            QString head = headers[i].toLower();
            // Normalise underscores/dashes to spaces for uniform matching
            head.replace('_', ' ').replace('-', ' ');

            if (head.contains("ci reference") || head == "ci ref"
                    || (head.contains("ci") && head.contains("ref"))) {
                if (ciRefCol == -1) ciRefCol = i; // keep first match
            } else if (head == "version" || head.contains("ci version")) {
                if (ciVerCol == -1) ciVerCol = i;
            } else if (head.contains("md5") || head.contains("checksum")) {
                if (md5Col == -1) md5Col = i;
            } else if (head.contains("document link") || head.contains("doc link")
                       || head.contains("link")) {
                if (docLinkCol == -1) docLinkCol = i;
            } else if (head.contains("crq") || head.contains("component")) {
                if (compCol == -1) compCol = i;
            }
        }

        // Fallback: if the CI Reference column still cannot be found, scan the
        // first data row for a cell that looks like a CI reference (contains a dash
        // and is at least 5 chars).  Column 5 is used only as a last resort.
        if (ciRefCol == -1) {
            if (rows.size() > 1) {
                QStringList firstData = parseCSVLine(rows[1], delimiter);
                for (int i = 0; i < firstData.size(); i++) {
                    QString cell = firstData[i].trimmed();
                    if (cell.contains('-') && cell.length() >= 5) {
                        ciRefCol = i;
                        logMessage(QString("CI Reference column not found in headers — auto-detected at column %1 from first data row").arg(i), "warning");
                        break;
                    }
                }
            }
            if (ciRefCol == -1 && headers.size() > 5) {
                ciRefCol = 5;
                logMessage("CI Reference column not found — falling back to column 5", "warning");
            }
        }

        // Dedup map: normalized key (lowercase + no spaces) -> first occurrence
        QMap<QString, int> seenKeys;

        // Parse every data row (skip row 0 which is the header)
        for (int r = 1; r < rows.size(); r++) {
            const QString& rawLine = rows[r];
            if (rawLine.trimmed().isEmpty()) continue;

            QStringList cells = parseCSVLine(rawLine, delimiter);

            if (ciRefCol < 0 || ciRefCol >= cells.size()) continue;

            QString ciRef = cells[ciRefCol].trimmed();
            // Strip surrounding quotes (RFC 4180)
            if (ciRef.startsWith("\"") && ciRef.endsWith("\"") && ciRef.length() >= 2) {
                ciRef = ciRef.mid(1, ciRef.length() - 2).trimmed();
            }
            // Clean embedded HTML tags from CI Reference values
            ciRef.replace(QRegExp("<[^>]+>"), "");
            ciRef = ciRef.trimmed();

            if (ciRef.isEmpty()) continue;

            // Dedup key: lowercase + remove all spaces
            QString normKey = ciRef.toLower();

            normKey.remove(' ');
            if (seenKeys.contains(normKey)) {
                continue; // duplicate, skip
            }
            seenKeys[normKey] = parsedItems.size();

            // Extract optional columns
            QString version, md5, docLink, component;
            if (ciVerCol != -1 && ciVerCol < cells.size()) {
                version = cells[ciVerCol].trimmed();
                if (version.startsWith("\"") && version.endsWith("\"")) version = version.mid(1, version.length() - 2).trimmed();
            }
            if (md5Col != -1 && md5Col < cells.size()) {
                md5 = cells[md5Col].trimmed();
                if (md5.startsWith("\"") && md5.endsWith("\"")) md5 = md5.mid(1, md5.length() - 2).trimmed();
            }
            if (docLinkCol != -1 && docLinkCol < cells.size()) {
                docLink = cells[docLinkCol].trimmed();
                if (docLink.startsWith("\"") && docLink.endsWith("\"")) docLink = docLink.mid(1, docLink.length() - 2).trimmed();
            }
            if (compCol != -1 && compCol < cells.size()) {
                component = cells[compCol].trimmed();
                if (component.startsWith("\"") && component.endsWith("\"")) component = component.mid(1, component.length() - 2).trimmed();
            }

            QMap<QString, QString> item;
            item["fileName"] = ciRef;
            item["version"] = version;
            item["expectedMd5"] = md5;
            item["documentLink"] = docLink;
            item["component"] = component;
            parsedItems.append(item);
        }
    }

    return parsedItems;
}

// Helper: match a VDD record against config items and resolve the file path
QString MainWindow::resolveFilePathForRecord(const VddRecord& rec, const QList<QMap<QString, QString>>& configItems) {
    QString fnKey = rec.fileName.toLower().trimmed();
    QString refKey = rec.ciReference.toLower().trimmed();
    QString refBase = QFileInfo(refKey).baseName();
    QString fnBase = QFileInfo(fnKey).baseName();

    // Build lookups
    QMap<QString, QString> fileLookup, baseLookup;
    for (const auto& item : configItems) {
        QString ciKey = item.value("fileName", "").trimmed().toLower();
        QString ciBase = QFileInfo(ciKey).baseName().toLower();
        fileLookup.insert(ciKey, item.value("documentLink", ""));
        if (!baseLookup.contains(ciBase))
            baseLookup.insert(ciBase, item.value("documentLink", ""));
    }

    QString docLink;

    // 1) Direct ciReference match
    if (fileLookup.contains(refKey)) docLink = fileLookup[refKey];
    // 2) Direct baseName match
    else if (baseLookup.contains(refBase)) docLink = baseLookup[refBase];
    // 3) Direct fileName match
    else if (fileLookup.contains(fnKey)) docLink = fileLookup[fnKey];
    // 4) Fuzzy baseName match
    else if (baseLookup.contains(fnBase)) docLink = baseLookup[fnBase];
    // 5) Fuzzy containment (safeguarded against short prefixes)
    else {
        for (auto it = fileLookup.begin(); it != fileLookup.end(); ++it) {
            QString configKey = it.key();
            QString configBase = QFileInfo(configKey).baseName().toLower();
            if ((refKey.length() >= 12 && configKey.length() >= 12 && (refKey.contains(configKey) || configKey.contains(refKey))) ||
                (refBase.length() >= 12 && configBase.length() >= 12 && (refBase.contains(configBase) || configBase.contains(refBase)))) {
                docLink = it.value();
                break;
            }
        }
    }

    if (docLink.isEmpty()) return "";

    QString docLinkClean = docLink.trimmed();
    QFileInfo directFi(docLinkClean);

    // Handle UNC paths for existence check
    auto netExists = [](const QString& path) -> bool {
        QString p = path;
        if (p.startsWith("\\\\")) p = "file:" + p;
        return QDir(p).exists();
    };

    bool isNetworkPath = docLinkClean.startsWith("\\\\");
    bool pathOk = isNetworkPath ? netExists(docLinkClean) : QDir(docLinkClean).exists();

    // Try 1: exact file path
    if (directFi.isFile() && directFi.exists()) return docLinkClean;

    // Try 2: treat as directory, search for file
    if ((directFi.isDir() || pathOk) && !docLinkClean.isEmpty()) {
        QDir searchDirObj(docLinkClean);
        if (!searchDirObj.exists()) {
            searchDirObj.setPath(docLinkClean.startsWith("\\\\") ? "file:" + docLinkClean : docLinkClean);
        }
        if (searchDirObj.exists()) {
            QFileInfo ciFi(rec.ciReference);
            QString ciBase = ciFi.baseName();
            QString ciSuffix = ciFi.suffix();
            QFileInfo recFi(rec.fileName);
            QStringList nameFilters;
            nameFilters << rec.fileName
                        << ciFi.fileName()
                        << ciBase + "." + ciSuffix
                        << ciBase
                        << ciBase + ".*"
                        << ".*"
                        << "*." + ciSuffix;
            // Deduplicate
            { QSet<QString> seen; QStringList deduped; for (const QString& n : nameFilters) { if (!seen.contains(n)) { seen.insert(n); deduped.append(n); } } nameFilters = deduped; }
            QFileInfoList fileList = searchDirObj.entryInfoList(nameFilters, QDir::Files);

            if (!fileList.isEmpty()) return fileList.first().absoluteFilePath();

            // Try case-insensitive match
            QStringList allFiles = searchDirObj.entryList(QDir::Files);
            for (const QString& f : allFiles) {
                QString checkBase = QFileInfo(f).baseName().toLower();
                checkBase.remove('.').remove('-').remove('_');
                QString checkCiBase = ciBase;
                checkCiBase.remove('.').remove('-').remove('_');
                if (checkBase == checkCiBase || f.toLower() == rec.fileName.toLower() || f.toLower().contains(ciBase.toLower())) {
                    return searchDirObj.absoluteFilePath(f);
                }
            }

            // Try recursive
            if (isNetworkPath) {
                QString netPath = "file:" + docLinkClean;
                QDirIterator subIt(netPath, QDirIterator::Subdirectories);
                while (subIt.hasNext()) {
                    QFileInfo subFi(subIt.next());
                    QString subBase = subFi.baseName().toLower();
                    subBase.remove('.').remove('-').remove('_');
                    QString ciBaseStripped = ciBase;
                    ciBaseStripped.remove('.').remove('-').remove('_');
                    if (subBase == ciBaseStripped || subFi.fileName().toLower() == rec.fileName.toLower() || subFi.fileName().toLower().contains(ciBase.toLower())) {
                        QString localPath = subFi.absoluteFilePath();
                        if (localPath.startsWith("file:")) localPath = localPath.mid(5);
                        return localPath;
                    }
                }
            }
        }
    }

    return "";
}

QString MainWindow::stripVersionSuffix(const QString& name) {
    QString res = name.trimmed().toLower();
    
    // Remove common file extensions
    if (res.endsWith(".zip")) res.chop(4);
    else if (res.endsWith(".7z")) res.chop(3);
    else if (res.endsWith(".exe")) res.chop(4);
    else if (res.endsWith(".tar")) res.chop(4);
    else if (res.endsWith(".tgz")) res.chop(4);
    else if (res.endsWith(".gz")) res.chop(3);
    else if (res.endsWith(".rar")) res.chop(4);
    
    // Remove all whitespace
    res.remove(' ');
    
    // Strip version patterns at the end:
    // Pattern 1: -<number><v/r/v./r.><number/letter> e.g. -1v00, -1v01, -2v00, -v2, -r1
    // Pattern 2: -<number>.<number> e.g. -1.00, -1.01
    // Pattern 3: -[vr][0-9]+ e.g. -v2, -r1
    QRegExp rx1("-[0-9]+[vVrR][0-9a-zA-Z]+$");
    QRegExp rx2("-[0-9]+\\.[0-9]+$");
    QRegExp rx3("-[vVrR][0-9]+$");
    
    if (res.contains(rx1)) {
        res.remove(rx1);
    } else if (res.contains(rx2)) {
        res.remove(rx2);
    } else if (res.contains(rx3)) {
        res.remove(rx3);
    }
    
    while (res.endsWith('-') || res.endsWith('_')) {
        res.chop(1);
    }
    
    return res;
}

// onVerifyFileClicked has been merged into onVerifyLocalClicked (now "Verify All")

QStringList MainWindow::parseCSV(const QString& filePath) {
    QStringList result;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return result;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");

    // RFC 4180-compliant reader: quoted fields may span multiple physical lines.
    // We accumulate lines until the running quote count is even (meaning we are
    // not inside an open quoted field) before committing a logical row.
    QString accumulated;
    int quoteCount = 0;

    while (!in.atEnd()) {
        QString line = in.readLine();
        for (QChar ch : line)
            if (ch == '"') quoteCount++;

        if (!accumulated.isEmpty()) accumulated += '\n';
        accumulated += line;

        if (quoteCount % 2 == 0) {
            if (!accumulated.trimmed().isEmpty())
                result << accumulated;
            accumulated.clear();
            quoteCount = 0;
        }
    }
    // Flush any unterminated quoted field at end-of-file
    if (!accumulated.trimmed().isEmpty())
        result << accumulated;

    file.close();
    return result;
}

QStringList MainWindow::parseCSVLine(const QString& line, QChar delim) {
    QStringList result;
    QString current;
    bool inQuotes = false;

    for (int i = 0; i < line.length(); i++) {
        QChar ch = line[i];
        if (ch == '"') {
            if (inQuotes && i + 1 < line.length() && line[i + 1] == '"') {
                current += '"';  // RFC 4180 escaped double-quote inside a quoted field
                i++;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (ch == delim && !inQuotes) {
            result << current.trimmed();
            current.clear();
        } else {
            current += ch;
        }
    }
    result << current.trimmed();
    return result;
}

QStringList MainWindow::parseXLSX(const QString& filePath) {
    // Try to parse XLSX as a ZIP-based format (since we don't have QtXlsx library,
    // we'll try reading the sharedStrings.xml and worksheets XML directly via our ZipReader)
    QStringList result;

    ZipReader zip(filePath);
    if (!zip.exists() || !zip.isReadable()) {
        return result;
    }

    // Extract shared strings (text values)
    QMap<int, QString> sharedStrings;
    QByteArray ssData = zip.fileData("xl/sharedStrings.xml");
    if (!ssData.isEmpty()) {
        QString ssXml = QString::fromUtf8(ssData);
        // Parse <t> tags from sharedStrings.xml
        QRegExp tRx("<t[^>]*>(.*?)</t>");
        int pos = 0;
        int idx = 0;
        while ((pos = tRx.indexIn(ssXml, pos)) != -1) {
            QString text = tRx.cap(1).trimmed();
            sharedStrings.insert(idx++, text);
            pos += tRx.matchedLength();
        }
    }

    // Extract first worksheet
    QByteArray sheetData = zip.fileData("xl/worksheets/sheet1.xml");
    if (!sheetData.isEmpty()) {
        QString sheetXml = QString::fromUtf8(sheetData);

        // Parse <row> elements
        QRegExp rowRx("<row[^>]*>(.*?)</row>");
        rowRx.setMinimal(true);
        int rowPos = 0;
        int currentRow = 0;
        QVector<QStringList> allRows;

        while ((rowPos = rowRx.indexIn(sheetXml, rowPos)) != -1) {
            QString rowContent = rowRx.cap(1);
            QStringList cells;

            // Parse <c r="..." t="..."><v>value</v></c> patterns
            QRegExp cellRx("<c[^>]*>(.*?)</c>");
            int cellPos = 0;
            while ((cellPos = cellRx.indexIn(rowContent, cellPos)) != -1) {
                QString cellContent = cellRx.cap(1);
                // Extract column index from r attribute (e.g., "A1", "B1")
                QRegExp colRx("r=\"([A-Z]+)\\d+\"");
                QString colLetter = "A";
                if (colRx.indexIn(cellContent) != -1) {
                    colLetter = colRx.cap(1);
                }
                int colIdx = excelColToIndex(colLetter);

                // Extract value
                QString value;
                QRegExp vRx("<v[^>]*>(.*?)</v>");
                if (vRx.indexIn(cellContent) != -1) {
                    QString v = vRx.cap(1).trimmed();
                    // Check if cell type is shared
                    QRegExp tAttrRx("t=\"shared\"");
                    if (tAttrRx.indexIn(cellContent) != -1 && !v.isEmpty()) {
                        if (sharedStrings.contains(v.toInt())) {
                            value = sharedStrings[v.toInt()];
                        }
                    } else {
                        value = v;
                    }
                } else {
                    // Try to get text from <is><t> tags (inline strings)
                    QRegExp isTx("<is><t[^>]*>(.*?)</t></is>");
                    if (isTx.indexIn(cellContent) != -1) {
                        value = isTx.cap(1).trimmed();
                    }
                }

                // Expand cells array to accommodate column index
                while (cells.size() <= colIdx) {
                    cells << "";
                }
                cells[colIdx] = value;

                cellPos += cellRx.matchedLength();
            }

            if (!cells.isEmpty()) {
                allRows << cells;
            }
            currentRow++;
            rowPos += rowRx.matchedLength();
        }

        // Convert rows to line-based format for consistent parsing
        bool isHeader = true;
        for (const QStringList& row : allRows) {
            if (row.isEmpty()) continue;

            // Skip empty rows (all cells empty)
            bool allEmpty = true;
            for (const QString& cell : row) {
                if (!cell.trimmed().isEmpty()) {
                    allEmpty = false;
                    break;
                }
            }
            if (allEmpty) continue;

            // Skip header row
            if (isHeader) {
                isHeader = false;
                bool looksLikeHeader = false;
                for (const QString& cell : row) {
                    QString lower = cell.toLower();
                    if (lower.contains("file") || lower.contains("name") || lower.contains("ci") ||
                        lower.contains("version") || lower.contains("md5") || lower.contains("checksum")) {
                        looksLikeHeader = true;
                        break;
                    }
                }
                if (looksLikeHeader) continue;
            }

            // Convert to a CSV-like line
            QString lineStr;
            for (int i = 0; i < row.size(); i++) {
                if (i > 0) lineStr += ",";
                lineStr += row[i].trimmed();
            }
            if (!lineStr.isEmpty() && lineStr != ",,") {
                result << lineStr;
            }
        }
    }

    return result;
}

int MainWindow::excelColToIndex(const QString& colStr) {
    int idx = 0;
    for (int i = 0; i < colStr.length(); i++) {
        idx = idx * 26 + (colStr[i].toUpper().unicode() - 'A' + 1);
    }
    return idx - 1; // 0-based
}

QString MainWindow::extractVersionFromDocument(const QString& filePath) {
    if (filePath.isEmpty()) return "";
    QString baseName = QFileInfo(filePath).baseName();

    // Priority 1: Look for version pattern at the end of the file name
    // Examples: "DP-CRF-6648-V1-AO-ATE-X86-W10-1V00" → "1V00"
    //           "DP-SPL-7014-V1-01-U19-R1-47CD" → "R1" (since 47CD is hash-like)
    //           "DP-CRF-6648-V1-TAB-ATE-X86-MariaDB10V11-1V00" → "1V00"
    // Pattern: last segment after the final dash that looks like a version
    QStringList parts = baseName.split('-');
    for (int i = parts.size() - 1; i >= 0; --i) {
        QString part = parts[i].trimmed().toUpper();
        // Skip pure hex/hash parts (all hex digits) and pure numeric parts
        if (part.isEmpty()) continue;
        if (part.contains(QRegExp("^[0-9A-Fa-f]{4,}$")) && part.contains(QRegExp("[A-F]"))) continue;
        if (part.contains(QRegExp("^[0-9]+$"))) continue; // pure numbers like "01"
        // Check if it looks like a version: contains both letter and digit, or R followed by digit
        if (part.contains(QRegExp("[A-Z]")) && part.contains(QRegExp("[0-9]"))) {
            return part;
        }
        // Also accept patterns like "V1", "R1", "V1V00"
        if (part.contains(QRegExp("^[Vv]\\d+[A-Za-z0-9]*$")) || part.contains(QRegExp("^R\\d+[A-Za-z0-9]*$"))) {
            return part.toUpper();
        }
    }

    // Priority 2: Search inside the full base name for version patterns
    // Prefer longer matches (later/larger version)
    QRegExp verRx("([Vv]\\d+[A-Za-z0-9.-]*\\d*[A-Za-z0-9]*|\\d+[Vv]\\d+[A-Za-z0-9.-]*|R\\d+[A-Za-z0-9.-]*\\d*[A-Za-z0-9]*)");
    QString bestMatch;
    int matchPos = 0;
    int pos = 0;
    while ((pos = verRx.indexIn(baseName, pos)) != -1) {
        QString candidate = verRx.cap(1).toUpper();
        int start = pos;
        pos += verRx.matchedLength();
        // Validate: must have at least one letter and one digit
        if (!candidate.contains(QRegExp("[A-Z]")) || !candidate.contains(QRegExp("[0-9]"))) continue;
        // Skip pure version numbers like "7.4.2.2" that are app/build versions
        if (candidate.contains(QRegExp("^[\\d.]+$"))) continue;
        // Prefer the last (longest) match
        if (candidate.size() > bestMatch.size() || (candidate.size() == bestMatch.size() && start > matchPos)) {
            bestMatch = candidate;
            matchPos = start;
        }
    }
    if (!bestMatch.isEmpty()) return bestMatch;

    return "";
}

// 5. Advanced UI Setting Panel Actions
void MainWindow::onOpenSettingsClicked() {
    SettingsDialog dialog(this);
    dialog.exec();
}

void MainWindow::onCrc32Toggled(bool checked) {
    m_tableWidget->setColumnHidden(14, !checked); // CRC-32 (VDD)
    m_tableWidget->setColumnHidden(15, !checked); // CRC-32 (Local Dir)
    m_tableWidget->setColumnHidden(16, !checked); // CRC-32 Result
    updateTableDisplay();
}

void MainWindow::onCustomContextMenuRequested(const QPoint& pos) {
    QTableWidgetItem* item = m_tableWidget->itemAt(pos);
    if (!item) return;
    int row = item->row();

    if (m_mainRowToDetailRow.values().contains(row)) return;

    QTableWidgetItem* fileNameItem = m_tableWidget->item(row, 0);
    if (!fileNameItem) return;
    QString targetFileName = fileNameItem->text();
    int recordId = -1;
    for (int id : m_records.keys()) {
        if (m_records[id].fileName == targetFileName) {
            recordId = id;
            break;
        }
    }
    if (recordId < 0) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background: rgba(15, 23, 42, 0.95); border: 1px solid rgba(99, 102, 241, 0.3); border-radius: 8px; }"
        "QMenu::item { padding: 8px 16px; color: #F8FAFC; }"
        "QMenu::item:selected { background: rgba(99, 102, 241, 0.2); }"
    );

    QAction* copyPathAct = menu.addAction("Copy Reference Filename");
    QAction* copyExpectedAct = menu.addAction("Copy Expected Hash (MD5)");
    QAction* copyCalculatedAct = menu.addAction("Copy Calculated Hash");
    menu.addSeparator();
    QAction* verifySelectedAct = menu.addAction("Verify Selected File");
    QAction* openLocationAct = menu.addAction("Open Local Directory Location");
    menu.addSeparator();
    QAction* toggleDetailsAct = menu.addAction(m_rowExpanded.value(row, false) ? "Collapse Details" : "Expand Details");

    QAction* selectedAction = menu.exec(m_tableWidget->viewport()->mapToGlobal(pos));
    if (!selectedAction) return;

    if (selectedAction == copyPathAct) {
        QApplication::clipboard()->setText(m_tableWidget->item(row, 0)->text());
    } else if (selectedAction == copyExpectedAct) {
        QApplication::clipboard()->setText(m_tableWidget->item(row, 11)->text());
    } else if (selectedAction == copyCalculatedAct) {
        QApplication::clipboard()->setText(m_tableWidget->item(row, 12)->text());
    } else if (selectedAction == toggleDetailsAct) {
        toggleRowDetails(row);
    } else if (selectedAction == verifySelectedAct) {
        if (m_csvFilePaths.isEmpty()) {
            QMessageBox::warning(this, "No Configuration File", "Load a Configuration Items file first to resolve file paths.");
            return;
        }
        if (m_records.contains(recordId)) {
            QList<QMap<QString, QString>> configItems = parseConfigFileItems(m_csvFilePaths);
            QString fullPath = resolveFilePathForRecord(m_records[recordId], configItems);
            if (fullPath.isEmpty()) {
                QMessageBox::warning(this, "Not Found", "Physical file not found in configuration Document Link paths.");
                return;
            }
            QThread* thread = new QThread(this);
            ChecksumWorker* worker = new ChecksumWorker(recordId, fullPath);
            worker->moveToThread(thread);
            connect(thread, &QThread::started, worker, &ChecksumWorker::process);
            connect(worker, &ChecksumWorker::started, this, &MainWindow::onHashStarted);
            connect(worker, &ChecksumWorker::fileProgress, this, &MainWindow::onHashProgress);
            connect(worker, &ChecksumWorker::finished, this, &MainWindow::onHashFinished);
            connect(worker, &ChecksumWorker::finished, thread, &QThread::quit);
            connect(worker, &ChecksumWorker::finished, worker, &ChecksumWorker::deleteLater);
            connect(thread, &QThread::finished, thread, &QThread::deleteLater);
            m_verifierThreads.insert(recordId, thread);
            thread->start();
        }
    } else if (selectedAction == openLocationAct) {
        if (m_records.contains(recordId) && !m_csvFilePaths.isEmpty()) {
            QList<QMap<QString, QString>> configItems = parseConfigFileItems(m_csvFilePaths);
            QString fullPath = resolveFilePathForRecord(m_records[recordId], configItems);
            if (!fullPath.isEmpty()) {
                QFileInfo fi(fullPath);
                if (fi.isDir() || !QFile::exists(fullPath)) {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(fullPath));
                } else {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
                }
            } else {
                QMessageBox::warning(this, "Not Found", "No Document Link path available for this record.");
            }
        }
    }
}

void MainWindow::onLLMQueryStarted(int) {
    // Only process import-related events (sourceId == 0)
    // Document Reviewer handles its own events via its own queryToken
    if (m_llmParseMode.isEmpty()) return;
    m_statusLabel->setText("Sending file content to AI LLM for structured data extraction...");
    qApp->processEvents();
}

void MainWindow::onLLMQueryProgress(const QString& status, int) {
    if (m_llmParseMode.isEmpty()) return;
    m_statusLabel->setText("AI LLM status: " + status);
    qApp->processEvents();
}

void MainWindow::onLLMQueryResult(const QString& result, int) {
    if (m_llmParseMode.isEmpty()) return;

    QList<VddRecord> records = parseLLMJsonResponse(result);
    logMessage(QString("parseLLMJsonResponse: total LLM records after filtering=%1").arg(records.size()), "info");
    for (const VddRecord& r : records) {
        logMessage(QString("parseLLMJsonResponse: KEPT record: id=%1 fileName=%2 ciRef=%3 version=%4 md5=%5 crc32=%6")
            .arg(r.id).arg(r.fileName).arg(r.ciReference).arg(r.version).arg(r.expectedMd5).arg(r.expectedCrc32), "info");
    }

    if (records.isEmpty()) {
        QMessageBox::warning(this, "No CI Items Found", "AI LLM was unable to extract configuration items from the VDD document.\n\nRaw LLM response:\n" + result.left(500) + "\n\nCheck that the document contains a table with CI References, Versions, and checksums (MD5 or CRC-32).");
        m_statusLabel->setText("LLM extraction returned no data.");
    } else {
        m_records.clear();
        for (const VddRecord& rec : records) {
            m_records.insert(rec.id, rec);
        }
        m_statusLabel->setText(QString("AI extracted %1 CI item(s) from VDD document.").arg(records.size()));
        logMessage(QString("VDD AI extracted %1 CI items").arg(records.size()), "info");
        for (const VddRecord& r : records) {
            if (r.fileName.toLower().contains("srs") || r.fileName.toLower().contains("sdd") || r.fileName.toLower().contains("vdd") || r.fileName.toLower().contains("stid")) {
                logMessage(QString("VDD AI record: id=%1 fileName=%2 ciRef=%3 md5=%4 crc32=%5")
                    .arg(r.id).arg(r.fileName).arg(r.ciReference).arg(r.expectedMd5).arg(r.expectedCrc32), "info");
            }
        }
        updateTableDisplay();

        // Enable verification button now that records exist
        if (m_verifyLocalBtn) m_verifyLocalBtn->setEnabled(true);
    }
    if (m_loadingDialog) m_loadingDialog->close();
    m_llmParseMode = "";
}

void MainWindow::onLLMQueryFailed(const QString& errMsg, int) {
    if (m_llmParseMode.isEmpty()) return;
    if (m_loadingDialog) m_loadingDialog->close();
    QMessageBox::warning(this, "LLM Query Failed", "AI extraction failed: " + errMsg);
    m_statusLabel->setText("LLM query returned failure. Check gateway URL and token in Settings.");
    m_llmParseMode = "";
}

void MainWindow::clearActionLog() {
    if (m_actionLogText) {
        m_actionLogText->clear();
    }
}

// Export Functions
void MainWindow::onExportToCsvClicked() {
    if (m_records.isEmpty()) {
        QMessageBox::warning(this, "No Data", "No records available to export. Please import a VDD file first.");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, "Export to CSV", "", "CSV Files (*.csv)");
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Export Error", "Could not open file for writing: " + file.errorString());
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");

    // Write header — full 20-column verification layout
    out << "ID,File Name (VDD),File Name (Local Dir),File Name Result,"
           "CI Ref (VDD),CI Ref (Local Dir),CI Ref (Config List),CI Ref Result,"
           "Version (VDD),Version (Local Dir),Version (Config List),Version Result,"
           "MD5 (VDD),MD5 (Local Dir),MD5 Result,"
           "CRC-32 (VDD),CRC-32 (Local Dir),CRC-32 Result,"
           "Document Link,Doc Link Result,Source,"
           "Local Status,File Status,SHA-1,CRC-32\n";

    // Write data rows
    for (auto recIt = m_records.begin(); recIt != m_records.end(); ++recIt) {
        VddRecord& rec = recIt.value();

        // File Name columns
        QString fileNameLocal = rec.localFileName.isEmpty() ? "N/A" : rec.localFileName;
        QString fileNameResult = "N/A";
        if (rec.localStatus == "MATCH" || rec.localStatus == "MISMATCH") fileNameResult = "FOUND";
        else if (rec.localStatus == "MISSING") fileNameResult = "NOT FOUND";
        else if (rec.localStatus == "ERROR") fileNameResult = "ERROR";

        // CI Ref columns
        QString ciRefLocal = rec.localCiRef.isEmpty() ? "N/A" : rec.localCiRef;
        QString ciRefConfig = rec.configFileName.isEmpty() ? "N/A" : rec.configFileName;
        QString ciRefResult = rec.ciRefCheck == "PASS" ? "MATCH" : (rec.ciRefCheck == "MISMATCH" ? "NOT MATCH" : "SKIP");

        // Version columns
        QString versionLocal = rec.localVersion.isEmpty() ? "N/A" : rec.localVersion;
        QString versionConfig = rec.configVersion.isEmpty() ? "N/A" : rec.configVersion;
        QString versionResult = rec.versionCheck == "PASS" ? "MATCH" : (rec.versionCheck == "MISMATCH" ? "NOT MATCH" : "SKIP");

        auto cleanHash = [](QString hash) -> QString {
            hash = hash.trimmed().toLower();
            if (hash.startsWith("0x")) {
                hash = hash.mid(2);
            }
            return hash;
        };

        // MD5 columns
        QString md5Local = rec.calculatedMd5.isEmpty() ? "Not calculated" : rec.calculatedMd5;
        QString md5Result = "N/A";
        if (rec.source == "CONFIG_ONLY") {
            if (!rec.calculatedMd5.isEmpty()) md5Result = "VERIFIED";
            else md5Result = "NOT CALCULATED";
        } else {
            if (rec.calculatedMd5.isEmpty()) md5Result = "N/A";
            else if (rec.expectedMd5.trimmed().isEmpty()) md5Result = "SKIP";
            else if (cleanHash(rec.expectedMd5) == cleanHash(rec.calculatedMd5)) md5Result = "MATCH";
            else md5Result = "NOT MATCH";
        }

        // CRC-32 columns
        QString crc32Local = rec.calculatedCrc32.isEmpty() ? "Not calculated" : rec.calculatedCrc32;
        QString crc32Result = "N/A";
        if (rec.source == "CONFIG_ONLY") {
            if (!rec.calculatedCrc32.isEmpty()) crc32Result = "CALCULATED";
            else crc32Result = "N/A";
        } else {
            if (rec.expectedCrc32.isEmpty() || rec.calculatedCrc32.isEmpty()) {
                crc32Result = "SKIP";
            } else if (cleanHash(rec.expectedCrc32) == cleanHash(rec.calculatedCrc32)) {
                crc32Result = "MATCH";
            } else {
                crc32Result = "NOT MATCH";
            }
        }

        // Document Link
        QString docLink = rec.configPath.isEmpty() ? "N/A" : rec.configPath;
        QString docLinkResult = rec.pathCheck == "PASS" ? "MATCH" : (rec.pathCheck == "MISMATCH" ? "NOT MATCH" : "SKIP");

        out << rec.id << ","
            << "\"" << rec.fileName << "\","
            << "\"" << fileNameLocal << "\","
            << fileNameResult << ","
            << "\"" << rec.ciReference << "\","
            << "\"" << ciRefLocal << "\","
            << "\"" << ciRefConfig << "\","
            << ciRefResult << ","
            << "\"" << rec.version << "\","
            << "\"" << versionLocal << "\","
            << "\"" << versionConfig << "\","
            << versionResult << ","
            << "\"" << rec.expectedMd5 << "\","
            << "\"" << md5Local << "\","
            << md5Result << ","
            << "\"" << rec.expectedCrc32 << "\","
            << "\"" << crc32Local << "\","
            << crc32Result << ","
            << "\"" << docLink << "\","
            << docLinkResult << ","
            << (rec.source == "CONFIG_ONLY" ? "Config-Only" : "VDD") << ","
            << rec.localStatus << ","
            << rec.fileStatus << ","
            << "\"" << rec.calculatedSha1 << "\","
            << "\"" << rec.calculatedCrc32 << "\"\n";
    }

    file.close();
    m_statusLabel->setText("Exported to CSV successfully: " + QFileInfo(fileName).fileName());
}

void MainWindow::onGeneratePdfClicked() {
    if (m_records.isEmpty()) {
        QMessageBox::warning(this, "No Data", "No records available for report generation. Please import a VDD file first.");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, "Generate PDF Report", "", "PDF Files (*.pdf)");
    if (fileName.isEmpty()) {
        return;
    }

    // Create a simple text-based report (would need QPrinter for full PDF)
    QString reportContent;
    QTextStream out(&reportContent);

    out << "VDD Audit System - Verification Report\n";
    out << "=====================================\n\n";
    out << "Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";

    // Calculate statistics
    int total = m_records.size();
    int match = 0, mismatch = 0, missing = 0, configOnly = 0, pending = 0;
    for (const VddRecord& rec : m_records.values()) {
        QString status = rec.localStatus.toUpper();
        if (status == "MATCH") match++;
        else if (status == "MISMATCH") mismatch++;
        else if (status == "MISSING") missing++;
        else if (status == "CONFIG_ONLY") configOnly++;
        else pending++;
    }

    out << "Summary Statistics:\n";
    out << "------------------\n";
    out << "Total Records: " << total << "\n";
    out << "Matched: " << match << " (" << QString::asprintf("%.1f%%", (match * 100.0) / qMax(1, total)) << ")\n";
    out << "Mismatched: " << mismatch << " (" << QString::asprintf("%.1f%%", (mismatch * 100.0) / qMax(1, total)) << ")\n";
    out << "Missing: " << missing << " (" << QString::asprintf("%.1f%%", (missing * 100.0) / qMax(1, total)) << ")\n";
    out << "Config-Only: " << configOnly << "\n";
    out << "Pending: " << pending << "\n\n";

    out << "Detailed Results:\n";
    out << "================\n\n";

    for (const VddRecord& rec : m_records.values()) {
        out << "ID: " << rec.id << "\n";
        out << "File: " << rec.fileName << "\n";
        out << "CI Reference: " << rec.ciReference << "\n";
        out << "Version: " << rec.version << "\n";
        out << "Expected MD5: " << rec.expectedMd5 << "\n";
        out << "Calculated MD5: " << rec.calculatedMd5 << "\n";
        if (!rec.calculatedSha1.isEmpty()) {
            out << "SHA-1: " << rec.calculatedSha1 << "\n";
        }
        if (!rec.calculatedCrc32.isEmpty()) {
            out << "CRC-32: " << rec.calculatedCrc32 << "\n";
        }
        out << "Status: " << rec.localStatus << "\n\n";
    }

    // Save as text file (simple PDF alternative)
    QString txtFileName = QFileInfo(fileName).baseName() + ".txt";
    QFile file(QFileInfo(fileName).absolutePath() + "/" + txtFileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream fileOut(&file);
        fileOut << reportContent;
        file.close();
        m_statusLabel->setText("Report generated: " + txtFileName + " (Text format)");
        QMessageBox::information(this, "Report Generated", "Report saved as:\n" + file.fileName() + "\n\nNote: Full PDF generation requires additional libraries. Text format provided.");
    } else {
        QMessageBox::critical(this, "Export Error", "Could not create report file.");
    }
}

QList<VddRecord> MainWindow::parseLLMJsonResponse(const QString& jsonString) {
    QList<VddRecord> records;

    // Trim and clean the response (remove possible markdown code fences)
    QString cleaned = jsonString.trimmed();
    if (cleaned.startsWith("```")) {
        int endMark = cleaned.indexOf("```", 3);
        if (endMark != -1) {
            cleaned = cleaned.mid(3, endMark - 3);
        }
    }
    cleaned = cleaned.trimmed();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(cleaned.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        return records;
    }

    if (!doc.isArray()) {
        return records;
    }

    int incId = m_llmImportTargetId;
    int totalInput = doc.array().size();
    QSet<QString> seenCiRefs;
    QSet<QString> seenFileNames;
    int skippedDedup = 0, skippedNoFilename = 0, kept = 0;
    for (const QJsonValue& val : doc.array()) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();
        QString fn = obj["fileName"].toString();
        QString ciRef = obj["ciReference"].toString().isEmpty() ? fn : obj["ciReference"].toString();

        // Deduplicate CI references and file names independently so that a record
        // whose fileName equals a prior record's ciReference is not falsely dropped.
        QString normKey = ciRef.toLower().trimmed();
        if (seenCiRefs.contains(normKey)) {
            skippedDedup++;
            continue;
        }
        seenCiRefs.insert(normKey);
        QString normFn = fn.toLower().trimmed();
        if (!normFn.isEmpty() && seenFileNames.contains(normFn)) {
            skippedDedup++;
            continue;
        }
        if (!normFn.isEmpty()) seenFileNames.insert(normFn);

        VddRecord rec;
        rec.id = incId++;
        rec.source = "VDD";
        rec.fileName = fn;
        rec.ciReference = ciRef;
        rec.version = obj["version"].toString();
        rec.expectedMd5 = obj["expectedMd5"].toString();
        rec.expectedCrc32 = obj["expectedCrc32"].toString();
        // Post-parse fixup: if LLM returned an 8-char value in expectedMd5, it's actually CRC32
        if (!rec.expectedMd5.isEmpty() && rec.expectedMd5.length() == 8 && rec.expectedMd5.contains(QRegExp("^[0-9a-fA-F]{8}$"))) {
            rec.expectedCrc32 = rec.expectedMd5;
            rec.expectedMd5 = "";
        }
        // Also check alternate keys for CRC32
        if (rec.expectedCrc32.isEmpty()) {
            QString altCrc = obj["crc32"].toString();
            if (altCrc.length() == 8 && altCrc.contains(QRegExp("^[0-9a-fA-F]{8}$"))) {
                rec.expectedCrc32 = altCrc;
            }
        }
        // Reject non-32-char MD5 values
        if (!rec.expectedMd5.isEmpty() && rec.expectedMd5.length() != 32) {
            rec.expectedMd5 = "";
        }
        rec.calculatedMd5 = "";
        rec.calculatedSha1 = "";
        rec.calculatedCrc32 = "";
        rec.localFileName = "";
        rec.localCiRef = "";
        rec.localVersion = "";
        rec.localFullPath = "";
        rec.configFileName = "";
        rec.configVersion = "";
        rec.localStatus = "PENDING";
        rec.fileStatus = "PENDING";
        bool include = !rec.fileName.isEmpty();
        if (!include) {
            skippedNoFilename++;
        } else {
            kept++;
        }
        if (include) {
            records.append(rec);
        }
    }
    logMessage(QString("parseLLMJsonResponse: LLM returned %1 items, dedup=%2 dropped, no_filename=%3 dropped, kept=%4")
        .arg(totalInput).arg(skippedDedup).arg(skippedNoFilename).arg(kept), "info");
    m_llmImportTargetId = incId;
    return records;
}

QList<QMap<QString, QString>> MainWindow::parseLLMFileResponse(const QString& jsonString) {
    QList<QMap<QString, QString>> items;

    QString cleaned = jsonString.trimmed();
    if (cleaned.startsWith("```")) {
        int endMark = cleaned.indexOf("```", 3);
        if (endMark != -1) {
            cleaned = cleaned.mid(3, endMark - 3);
        }
    }
    cleaned = cleaned.trimmed();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(cleaned.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        return items;
    }

    if (!doc.isArray()) {
        return items;
    }

    for (const QJsonValue& val : doc.array()) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();

        // Normalize: prefer "fileName", fall back to "file_name"
        QString fn = obj["fileName"].toString();
        if (fn.isEmpty()) {
            fn = obj["file_name"].toString();
        }
        if (fn.isEmpty()) {
            fn = obj["filename"].toString();
        }

        // Also check if it has CI reference info
        QString version = obj["version"].toString();

        // Parse CRC-32 first: only accept if exactly 8 hex characters
        QString crc32 = obj["expectedCrc32"].toString();
        if (crc32.isEmpty()) crc32 = obj["crc32"].toString();
        if (crc32.isEmpty()) crc32 = obj["checksumCrc32"].toString();
        if (!crc32.isEmpty() && crc32.length() != 8) {
            crc32 = ""; // reject non-8-char values as CRC32
        }

        // Parse MD5: only accept if exactly 32 hex characters
        QString md5 = obj["expectedMd5"].toString();
        if (md5.isEmpty()) md5 = obj["md5"].toString();
        if (md5.isEmpty()) md5 = obj["checksum"].toString();
        // If the extracted "md5" is actually 8 chars, it's CRC32 — move it
        if (!md5.isEmpty() && md5.length() == 8 && md5.contains(QRegExp("^[0-9a-fA-F]{8}$"))) {
            crc32 = md5;
            md5 = "";
        }
        // If extracted "md5" is not 32 hex chars and not empty, reject it
        if (!md5.isEmpty() && md5.length() != 32) {
            md5 = "";
        }

        if (!fn.isEmpty()) {
            QMap<QString, QString> item;
            item["fileName"] = fn;
            item["version"] = version;
            item["expectedMd5"] = md5;
            item["expectedCrc32"] = crc32;
            const auto keys = obj.keys();
            for (const QString& k : keys) {
                if (k != "fileName" && k != "file_name" && k != "filename" &&
                    k != "version" && k != "expectedMd5" && k != "md5" && k != "checksum" &&
                    k != "expectedCrc32" && k != "crc32" && k != "checksumCrc32" &&
                    k != "md5Check" && k != "crc32Check" && k != "versionCheck" && k != "ciRefCheck" && k != "compCheck" && k != "pathCheck") {
                    item[k] = obj[k].toString();
                }
            }
            items.append(item);
        }
    }
    return items;
}

void MainWindow::verifyAgainstParsedItems(const QList<QMap<QString, QString>>& parsedItems) {
    // Build lookup by fileName AND by ciReference/component for cross-matching
    // The CSV stores CI Reference (e.g. DP-CRF-6648-V1-VDD-ATE-1V00) in "fileName"
    // The VDD stores the same value in ciReference, but the file name has extension
    QMap<QString, QMap<QString, QString>> fileLookup;      // key -> item
    QMap<QString, QMap<QString, QString>> baseLookup;      // baseName -> item
    QMap<QString, QList<QMap<QString, QString>>> refLookup;  // ciReference/component -> items
    QMap<QString, QList<QMap<QString, QString>>> spaceStrippedLookup;  // no-space -> items
    for (const auto& item : parsedItems) {
        QString fnKey = item.value("fileName", "").trimmed().toLower();
        QString baseKey;
        if (!fnKey.isEmpty()) {
            baseKey = QFileInfo(fnKey).baseName().toLower();
            fileLookup.insert(fnKey, item);
            // Keep first entry for each base name — later duplicates are tracked via refLookup
            if (!baseLookup.contains(baseKey))
                baseLookup.insert(baseKey, item);
        }
        QString refKey = item.value("ciReference", item.value("component", "")).trimmed().toLower();
        if (!refKey.isEmpty()) {
            refLookup[refKey].append(item);
        }
        // Also add a space-stripped key for fuzzy matching
        QString strippedKey = fnKey;
        strippedKey.remove(' ');
        if (!strippedKey.isEmpty()) {
            spaceStrippedLookup[strippedKey].append(item);
        }
    }

    int matched = 0, notInFile = 0, mismatch = 0;

    for (int id : m_records.keys()) {
        VddRecord updated = m_records[id];

        // Skip CONFIG_ONLY records — already handled in Phase 2.5
        if (updated.source == "CONFIG_ONLY") {
            continue;
        }

        QString fnKey = updated.fileName.toLower().trimmed();
        QString refKey = updated.ciReference.toLower().trimmed();
        QString refBase = QFileInfo(refKey).baseName();
        QString fnBase = QFileInfo(fnKey).baseName();

        bool found = false;
        QMap<QString, QString> matchedItem;

        // 1) Direct ciReference match (VDD ciReference == CSV ciReference refKey)
        if (!found && refLookup.contains(refKey)) {
            found = true;
            matchedItem = refLookup[refKey].first();
        }
        // 1b) Substring containment match — both sides must be at least 8 chars to
        //     prevent short tokens like "SRS" or "VDD" from matching unrelated items.
        if (!found && refKey.length() >= 8) {
            for (auto it = refLookup.begin(); it != refLookup.end(); ++it) {
                QString rk = it.key();
                if (rk.length() >= 8 && (refKey.contains(rk) || rk.contains(refKey))) {
                    found = true;
                    matchedItem = it.value().first();
                    break;
                }
            }
        }
        // 2) Direct baseName match
        if (!found && baseLookup.contains(refBase)) {
            found = true;
            matchedItem = baseLookup[refBase];
        }
        // 3) Direct fileName match (VDD fileName == CSV fileName)
        if (!found && fileLookup.contains(fnKey)) {
            found = true;
            matchedItem = fileLookup[fnKey];
        }
        // 4) Fuzzy baseName match
        if (!found && baseLookup.contains(fnBase)) {
            found = true;
            matchedItem = baseLookup[fnBase];
        }
        // 5) Fuzzy: VDD ciReference baseName contained in or contains CSV fileName (safeguarded against short prefixes)
        if (!found) {
            for (auto it = fileLookup.begin(); it != fileLookup.end(); ++it) {
                QString configKey = it.key();
                QString configBase = QFileInfo(configKey).baseName().toLower();
                if ((refKey.length() >= 12 && configKey.length() >= 12 && (refKey.contains(configKey) || configKey.contains(refKey))) ||
                    (refBase.length() >= 12 && configBase.length() >= 12 && (refBase.contains(configBase) || configBase.contains(refBase)))) {
                    found = true;
                    matchedItem = it.value();
                    break;
                }
            }
        }
        // 6) Space-stripped match: handle "US- ADCAUTO" vs "US-ADCAUTO"
        if (!found) {
            QString refKeyNoSpace = refKey;
            refKeyNoSpace.remove(' ');
            if (spaceStrippedLookup.contains(refKeyNoSpace)) {
                for (const auto& candidate : spaceStrippedLookup[refKeyNoSpace]) {
                    QString candBase = QFileInfo(candidate.value("fileName", "")).baseName().toLower();
                    candBase.remove(' ');
                    QString refBaseNoSpace = refBase;
                    refBaseNoSpace.remove(' ');
                    if (refBaseNoSpace.contains(candBase) || candBase.contains(refBaseNoSpace)) {
                        found = true;
                        matchedItem = candidate;
                        break;
                    }
                }
            }
        }
        // 7) Suffix-stripped match: handle version differences or trailing suffix differences
        if (!found) {
            QString refKeySuffixStripped = stripVersionSuffix(refKey);
            for (const auto& item : parsedItems) {
                QString itemFn = item.value("fileName", "").trimmed().toLower();
                QString itemRef = item.value("ciReference", item.value("component", "")).trimmed().toLower();
                
                if (stripVersionSuffix(itemFn) == refKeySuffixStripped ||
                    stripVersionSuffix(itemRef) == refKeySuffixStripped ||
                    stripVersionSuffix(QFileInfo(itemFn).baseName()) == stripVersionSuffix(refBase) ||
                    stripVersionSuffix(QFileInfo(itemRef).baseName()) == stripVersionSuffix(refBase)) {
                    found = true;
                    matchedItem = item;
                    break;
                }
            }
        }

        if (found) {
            QString fileVersion = matchedItem.value("version", "").trimmed().toLower();
            QString fileMd5 = matchedItem.value("expectedMd5", "").trimmed().toLower();
            QString fileCrc32 = matchedItem.value("expectedCrc32", "").trimmed().toLower();

            // Extract config path and component audit metrics
            QString configPath = matchedItem.value("documentLink", "").trimmed();
            QString configComponent = matchedItem.value("component", "").trimmed();
            bool pathExists = matchedItem.value("pathExists", "false") == "true";
            bool fileFoundAtPath = matchedItem.value("fileFoundAtPath", "false") == "true";

            updated.configPath = configPath;
            updated.configPathExists = pathExists;
            updated.configFileFoundAtPath = fileFoundAtPath;
            updated.configFileName = matchedItem.value("fileName", "").trimmed();
            updated.configVersion = matchedItem.value("version", "").trimmed();

            QString recVersionClean = updated.version.trimmed().toLower();
            QString fileVersionClean = fileVersion;

            // Clean versions for comparison (remove common non-alphanumeric separators and 'v'/'V')
            recVersionClean.remove('.');
            recVersionClean.remove('-');
            recVersionClean.remove('_');
            recVersionClean.remove('v');
            fileVersionClean.remove('.');
            fileVersionClean.remove('-');
            fileVersionClean.remove('_');
            fileVersionClean.remove('v');

            bool versionOk = fileVersionClean.isEmpty() || recVersionClean.isEmpty() || 
                             recVersionClean.contains(fileVersionClean) || fileVersionClean.contains(recVersionClean);
            
            auto cleanHash = [](QString hash) -> QString {
                hash = hash.trimmed().toLower();
                if (hash.startsWith("0x")) {
                    hash = hash.mid(2);
                }
                return hash;
            };

            // If the configuration file lists an expected MD5, verify it matches
            bool md5Ok = fileMd5.isEmpty() || cleanHash(updated.expectedMd5) == cleanHash(fileMd5);

            // CRC-32 cross-validation
            bool crc32Ok = fileCrc32.isEmpty() || cleanHash(updated.expectedCrc32) == cleanHash(fileCrc32);

            // Specified path check: if a path is specified, it must exist and the corresponding file must be found inside
            bool pathOk = configPath.isEmpty() || (pathExists && fileFoundAtPath);

            // Cross-verify CI Reference between VDD and config file
            // The CSV stores CI Reference in "fileName", which should match VDD's ciReference
            QString csvCiRef = matchedItem.value("fileName", "").trimmed();
            bool ciMatch = csvCiRef.isEmpty() || updated.ciReference == csvCiRef
                || updated.ciReference.toLower().trimmed().endsWith(csvCiRef.toLower())
                || csvCiRef.toLower().endsWith(updated.ciReference.toLower().trimmed())
                || QFileInfo(updated.ciReference).baseName().toLower() == QFileInfo(csvCiRef).baseName().toLower()
                || updated.ciReference.toLower().trimmed().contains(csvCiRef.toLower())
                || csvCiRef.toLower().contains(updated.ciReference.toLower().trimmed());

            // Component (CRQ#) check: CSV component (e.g. CMB-HDD-9413) is a build reference,
            // not directly comparable to VDD ciReference. Mark SKIP if not available.
            QString compFromConfig = matchedItem.value("component", "").trimmed();
            bool compMatch = compFromConfig.isEmpty() || (updated.ciReference.contains(compFromConfig) || compFromConfig.contains(updated.ciReference));

            // Set individual check statuses
            if (!csvCiRef.isEmpty()) {
                updated.ciRefCheck = ciMatch ? "PASS" : "MISMATCH";
            } else {
                updated.ciRefCheck = "SKIP";
            }
            if (!compFromConfig.isEmpty()) {
                updated.compCheck = compMatch ? "PASS" : "MISMATCH";
            } else {
                updated.compCheck = "SKIP";
            }
            // Save the original VDD version BEFORE any overwrite — used in error messages
            QString originalVddVersion = updated.version;

            // Use config file version when available (more accurate), keep VDD version as fallback
            if (!fileVersionClean.isEmpty()) {
                updated.version = matchedItem.value("version", "").trimmed();
            }

            updated.versionCheck = versionOk ? "PASS" : "MISMATCH";
            if (!fileMd5.isEmpty()) {
                updated.md5Check = md5Ok ? "PASS" : "MISMATCH";
            } else {
                updated.md5Check = "SKIP";
            }
            if (!fileCrc32.isEmpty()) {
                updated.crc32Check = crc32Ok ? "PASS" : "MISMATCH";
            } else {
                updated.crc32Check = "SKIP";
            }
            if (!configPath.isEmpty()) {
                updated.pathCheck = pathOk ? "PASS" : "MISMATCH";
            } else {
                updated.pathCheck = "SKIP";
            }

            if (!ciMatch && !csvCiRef.isEmpty()) {
                updated.fileStatusReason = QString("CI Reference mismatch: VDD lists '%1', but configuration file lists '%2'.").arg(updated.ciReference).arg(csvCiRef);
            } else if (!compMatch && !compFromConfig.isEmpty()) {
                updated.fileStatusReason = QString("Component mismatch: VDD lists CI '%1', but configuration file component is '%2'.").arg(updated.ciReference).arg(compFromConfig);
            } else if (!versionOk) {
                updated.fileStatusReason = QString("Discrepancy: VDD lists version '%1', but configuration file lists version '%2'.").arg(originalVddVersion).arg(matchedItem.value("version"));
            } else if (!md5Ok) {
                updated.fileStatusReason = QString("Discrepancy: VDD expected MD5 '%1' differs from configuration expected MD5 '%2'.").arg(updated.expectedMd5).arg(matchedItem.value("expectedMd5"));
            } else if (!configPath.isEmpty() && !pathExists) {
                updated.fileStatusReason = QString("Discrepancy: The specified link path '%1' does not exist on disk or is not accessible.").arg(configPath);
            } else if (!configPath.isEmpty() && !fileFoundAtPath) {
                updated.fileStatusReason = QString("Discrepancy: Specified directory path exists, but corresponding file '%1' is missing from that directory.").arg(updated.fileName);
            } else {
                updated.fileStatusReason = QString("Component verified: Reference found in configuration file, and both version and path audits passed successfully.");
            }

            // Determine match status: accept if version + path OK, and all present checksums match
            // If only CRC-32 is present (no MD5), accept if CRC-32 matches
            // If both are present, both must match
            // If neither is present, still MATCH on version+path
            bool hasMd5 = !fileMd5.isEmpty() || !updated.expectedMd5.trimmed().isEmpty();
            bool hasCrc32 = !fileCrc32.isEmpty() || !updated.expectedCrc32.trimmed().isEmpty();
            bool checksumsOk = true;
            if (hasMd5 && !md5Ok) checksumsOk = false;
            if (hasCrc32 && !crc32Ok) checksumsOk = false;

            if (versionOk && pathOk && checksumsOk) {
                updated.fileStatus = "MATCH";
                matched++;
                logMessage(QString("Verified config item '%1': version and path checks passed.").arg(updated.ciReference), "success");
            } else {
                updated.fileStatus = "MISMATCH";
                mismatch++;
                logMessage(updated.fileStatusReason, "error");
            }
        } else {
            updated.fileStatus = "NOT_IN_FILE";
            updated.configPath = "";
            updated.configPathExists = false;
            updated.configFileFoundAtPath = false;
            updated.configFileName = "";
            updated.configVersion = "";
            updated.fileStatusReason = QString("VDD component reference '%1' (CI '%2', version %3) was not found in the loaded configuration list.").arg(updated.fileName).arg(updated.ciReference).arg(updated.version);
            updated.ciRefCheck = "SKIP";
            updated.compCheck = "SKIP";
            updated.versionCheck = "SKIP";
            updated.md5Check = "SKIP";
            updated.crc32Check = "SKIP";
            updated.pathCheck = "SKIP";
            notInFile++;
            logMessage(updated.fileStatusReason, "warning");
        }
        m_records[id] = updated;
    }

    updateTableDisplay();

    m_statusLabel->setText(QString("Local configuration file verification complete. Matched: %1, Mismatch: %2, Not in config: %3")
        .arg(matched).arg(mismatch).arg(notInFile));
}

// 6. Native Drag and Drop Implementation
void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            QString localPath = urls.first().toLocalFile();
            if (localPath.endsWith(".odt", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                onImportVddClicked();
            } else {
                QMessageBox::warning(this, "Unsupported File Format", "Only standard OpenDocument text documents (.odt) represent clean parse target formats.");
            }
        }
    }
}

void MainWindow::updateGuideState() {
    // Guide banner removed from UI; tour still available via toolbar action
}

void MainWindow::startGuidedTour() {
    showTourStep(0);
}

void MainWindow::showTourStep(int step) {
    m_tourStep = step;
    if (!m_tourCard) {
        m_tourCard = new QFrame(this);
        m_tourCard->setObjectName("tourCard");
        m_tourCard->setStyleSheet(
            "QFrame#tourCard { "
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1E1B4B, stop:1 #0F172A);"
            "  border: 2px solid #818CF8;"
            "  border-radius: 12px;"
            "  margin: 0px;"
            "  padding: 12px;"
            "}"
        );
        
        QVBoxLayout* l = new QVBoxLayout(m_tourCard);
        l->setSpacing(10);
        
        // Title / Header
        QHBoxLayout* h = new QHBoxLayout();
        QLabel* title = new QLabel("✨ INTERACTIVE GUIDED TOUR", m_tourCard);
        title->setStyleSheet("font-weight: 800; font-size: 11px; color: #818CF8;");
        QPushButton* close = new QPushButton("×", m_tourCard);
        close->setStyleSheet("QPushButton { color: #A5F3FC; border: none; font-size: 14px; font-weight: bold; background: transparent; } QPushButton:hover { color: white; }");
        connect(close, &QPushButton::clicked, this, &MainWindow::endGuidedTour);
        h->addWidget(title);
        h->addStretch();
        h->addWidget(close);
        l->addLayout(h);
        
        // Description Label
        m_tourText = new QLabel(m_tourCard);
        m_tourText->setStyleSheet("font-size: 11px; color: #F1F5F9; line-height: 1.3;");
        m_tourText->setWordWrap(true);
        l->addWidget(m_tourText);
        
        // Navigation Buttons Row
        QHBoxLayout* btns = new QHBoxLayout();
        m_tourSkipBtn = new QPushButton("Skip Tour", m_tourCard);
        m_tourSkipBtn->setStyleSheet("QPushButton { background: transparent; color: #94A3B8; border: none; font-size: 10px; font-weight: bold; } QPushButton:hover { color: #E2E8F0; }");
        connect(m_tourSkipBtn, &QPushButton::clicked, this, &MainWindow::endGuidedTour);
        
        m_tourBackBtn = new QPushButton("< Back", m_tourCard);
        m_tourBackBtn->setStyleSheet("QPushButton { background: rgba(255,255,255,0.05); color: #E2E8F0; border: 1px solid rgba(255,255,255,0.1); border-radius: 4px; padding: 4px 8px; font-size: 10px; font-weight: bold; } QPushButton:hover { background: rgba(255,255,255,0.1); }");
        connect(m_tourBackBtn, &QPushButton::clicked, this, [this]() { showTourStep(m_tourStep - 1); });
        
        m_tourNextBtn = new QPushButton("Next Step >", m_tourCard);
        m_tourNextBtn->setStyleSheet("QPushButton { background: #4F46E5; color: white; border-radius: 4px; padding: 4px 10px; font-size: 10px; font-weight: bold; } QPushButton:hover { background: #6366F1; }");
        connect(m_tourNextBtn, &QPushButton::clicked, this, [this]() { showTourStep(m_tourStep + 1); });
        
        btns->addWidget(m_tourSkipBtn);
        btns->addStretch();
        btns->addWidget(m_tourBackBtn);
        btns->addWidget(m_tourNextBtn);
        l->addLayout(btns);
        
        m_tourCard->resize(320, 160);
    }
    
    m_tourCard->show();
    m_tourCard->raise();
    
    // Reset all highlights from previous steps
    m_configFileEdit->setStyleSheet("");
    m_importVddBtn->setStyleSheet("");
    m_verifyLocalBtn->setStyleSheet("");
    m_actionLogDock->setStyleSheet("");

    // Restore the main tab widget styling (remove tour highlights)
    if (m_mainTabWidget) {
        m_mainTabWidget->setDocumentMode(true);
        m_mainTabWidget->setStyleSheet("");
    }

    QWidget* targetWidget = nullptr;
    QString text;

    switch(step) {
        case 0: // Welcome step
            text = "<h3>Welcome to the VDD Automated Audit Dashboard!</h3>This premium, AI-powered workspace allows you to parse, audit, and cross-reference dynamic system configuration registry files in seconds.<br/><br/>Let's walk you through the core components.";
            m_tourBackBtn->hide();
            m_tourNextBtn->setText("Start Tour >");
            // Position in center of MainWindow
            m_tourCard->move((width() - m_tourCard->width())/2, (height() - m_tourCard->height())/2);
            break;

        case 1: // Step 1: AI Settings
            text = "<b>Step 1: AI Configuration Credentials</b><br/>Configure your secure Anthropic LLM parameters by clicking the <b>Settings (Gear Icon)</b> on the toolbar to enable AI parsing of VDD documents.";
            m_tourBackBtn->show();
            m_tourNextBtn->setText("Next >");
            // position top left under toolbar settings roughly
            m_tourCard->move(20, 80);
            break;

        case 2: // Step 2: VDD Ingestion
            text = "<b>Step 2: Parse VDD Document</b><br/>Drag and drop your VDD <code>.odt</code> file anywhere or click the blue <b>Import & Analyze VDD</b> button to extract CI items via AI.";
            m_tourBackBtn->show();
            m_tourNextBtn->setText("Next >");
            targetWidget = m_importVddBtn;
            m_importVddBtn->setStyleSheet("background-color: #3B82F6; color: white; font-weight: bold; padding: 6px; border: 2px solid #818CF8;");
            break;

        case 3: // Step 3: Load Configuration Lists (was Step 4)
            text = "<b>Step 3: Load Configuration Lists</b><br/>Browse and select target CSV or Excel files listing configuration items with Document Links for path-mapping and hashing.";
            m_tourBackBtn->show();
            m_tourNextBtn->setText("Next >");
            targetWidget = m_configFileEdit;
            m_configFileEdit->setStyleSheet("border: 2px solid #818CF8; background: rgba(99,102,241,0.1); color: #FFF;");
            break;

        case 4: // Step 4: Verification
            text = "<b>Step 4: Verify All</b><br/>Click <b>Verify All</b> to perform both CI metadata verification (version, MD5, component against config) and physical file hashing (MD5/SHA1/CRC32 using Document Link paths) in a single operation.";
            m_tourBackBtn->show();
            m_tourNextBtn->setText("Next >");
            targetWidget = m_verifyLocalBtn;
            m_verifyLocalBtn->setStyleSheet("background-color: #4F46E5; color: white; font-weight: bold; padding: 6px; border: 2px solid #818CF8;");
            break;

        case 5: // Step 5: Console
            text = "<b>Step 5: Real-time System Verification Log</b><br/>All real-time audit notifications, warnings, discrepancies, and success alerts are reported directly in this bottom developer console.";
            m_tourBackBtn->show();
            m_tourNextBtn->setText("Finish Tour");
            targetWidget = m_actionLogDock;
            m_actionLogDock->setStyleSheet("QDockWidget::title { background: #818CF8; color: #000; }");
            break;

        default:
            endGuidedTour();
            return;
    }

    m_tourText->setText(text);
    
    if (targetWidget) {
        QPoint pos = targetWidget->mapTo(this, QPoint(0, 0));
        if (pos.y() + targetWidget->height() + m_tourCard->height() + 20 < height()) {
            m_tourCard->move(qMax(10, qMin(width() - m_tourCard->width() - 10, pos.x())), pos.y() + targetWidget->height() + 10);
        } else {
            m_tourCard->move(qMax(10, qMin(width() - m_tourCard->width() - 10, pos.x())), pos.y() - m_tourCard->height() - 10);
        }
    }
}

void MainWindow::endGuidedTour() {
    m_tourStep = -1;
    if (m_tourCard) {
        m_tourCard->hide();
    }
    m_configFileEdit->setStyleSheet("");
    m_importVddBtn->setStyleSheet("");
    m_verifyLocalBtn->setStyleSheet("");
    m_actionLogDock->setStyleSheet("");
    if (m_mainTabWidget) {
        m_mainTabWidget->setDocumentMode(true);
        m_mainTabWidget->setStyleSheet("");
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    Q_UNUSED(watched)
    Q_UNUSED(event)
    return QMainWindow::eventFilter(watched, event);
}

