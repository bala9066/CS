#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QSlider>
#include <QComboBox>
#include <QProgressBar>
#include <QLabel>
#include <QCheckBox>
#include <QTableWidget>
#include <QMap>
#include <QDialog>
#include <QPushButton>
#include <QProgressDialog>
#include <QTimer>
#include <QMutex>

#include <cmath>
#include <QDirIterator>
#include <QThread>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDockWidget>
#include <QTextEdit>
#include <QPointer>
#include <QTabWidget>
#include <QWidget>
#include <QSplitter>
#include <QRegularExpression>
#include <QStackedWidget>


// Forward declarations
class StatisticsWidget;
class BulkChecksumDock;
class VddDocumentReviewer;
class QFrame;

struct VddRecord {
    int id;
    QString fileName;
    QString ciReference;
    QString version;
    QString expectedMd5;
    QString calculatedMd5;
    QString calculatedSha1;      // New field for SHA-1 hash
    QString calculatedCrc32;    // New field for CRC32 checksum
    QString localFileName;      // Actual file name found in local directory
    QString localCiRef;         // CI reference derived from local file base name
    QString localVersion;       // Version extracted from ODT/DOCX document metadata
    QString localFullPath;      // Full path to the matched local file
    QString configFileName;     // CI reference / file name from config file parsing
    QString configVersion;      // Version from config file parsing
    QString localStatus; // "PENDING", "MATCH", "MISMATCH", "MISSING"
    QString fileStatus;  // "PENDING", "MATCH", "MISMATCH", "MISSING", "NOT_IN_FILE"
    QString devEnv;
    QString runtimeEnv;
    
    // Config path verification metrics
    QString configPath;
    QString configComponent;
    bool configPathExists = false;
    bool configFileFoundAtPath = false;

    // Detailed reasons for mismatch / missing / not in file statuses
    QString localStatusReason;
    QString fileStatusReason;

    // Individual check statuses for File Verify split columns
    QString ciRefCheck = "SKIP";
    QString compCheck = "SKIP";
    QString versionCheck = "SKIP";
    QString md5Check = "SKIP";
    QString pathCheck = "SKIP";

    QString expectedCrc32;
    QString crc32Check = "SKIP";

    QString source; // "VDD" or "CONFIG_ONLY"
};

// Sleek Modal Settings Dialog for LLM Gateway Integration
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() = default;

private slots:
    void onDiscoverModelsClicked();
    void onSaveSettingsClicked();
    void onModelsPopulated(const QStringList& models);
    void onModelDiscoveryFailed(const QString& errMsg);

private:
    QLineEdit* m_endpointEdit;
    QLineEdit* m_accessTokenEdit;
    QSlider* m_tempSlider;
    QLabel* m_tempValueLabel;
    QLineEdit* m_tokenLimitEdit;
    QComboBox* m_modelCombo;
    QPushButton* m_discoverBtn;
    QPushButton* m_saveBtn;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // High-fidelity logging helper
    void logMessage(const QString& msg, const QString& type = "info");

private slots:
    // Core Workflow Buttons
    void onImportVddClicked();
    void onVerifyLocalClicked();

    // Export Actions
    void onExportToCsvClicked();
    void onGeneratePdfClicked();

    // Settings & Options
    void onOpenSettingsClicked();
    void onCrc32Toggled(bool checked);

    void onCustomContextMenuRequested(const QPoint& pos);

    // Dynamic Hashing Handlers
    void onHashStarted(int recordId);
    void onHashProgress(int recordId, int progress);
    void onHashFinished(int recordId, const QString& calculatedMd5, const QString& calculatedSha1, const QString& calculatedCrc32, bool success, const QString& errorStr);

    // LLM-based extraction
    void onLLMQueryStarted(int sourceId = 0);
    void onLLMQueryProgress(const QString& status, int sourceId = 0);
    void onLLMQueryResult(const QString& result, int sourceId = 0);
    void onLLMQueryFailed(const QString& errMsg, int sourceId = 0);
    QList<VddRecord> parseLLMJsonResponse(const QString& jsonString);
    QList<QMap<QString, QString>> parseLLMFileResponse(const QString& jsonString);
    void verifyAgainstParsedItems(const QList<QMap<QString, QString>>& parsedItems);

    // Progress throttling
    void updateProgressSlow();

    // Collapsible Hash Details
    void toggleRowDetails(int row);
    void createDetailRow(int mainRow, const VddRecord& record);

    // Drag & Drop
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void initUi();
    QString extractVersionFromDocument(const QString& filePath);
    QList<QMap<QString, QString>> parseConfigFileItems(const QStringList& filePaths);
    QString resolveFilePathForRecord(const VddRecord& rec, const QList<QMap<QString, QString>>& configItems);
    QString stripVersionSuffix(const QString& name);
    void loadStyleSheet();
    void updateTableDisplay();
    void setRowStatusColors(int row, const QString& localStatus, const QString& webStatus);
    
    // Core Widgets
    QStackedWidget* m_tableStack;
    QWidget* m_welcomeWidget;
    QTableWidget* m_tableWidget;
    StatisticsWidget* m_statisticsWidget;

    // Bulk Checksum tab content
    BulkChecksumDock* m_bulkChecksumDock;
    // VDD Document Reviewer tab content
    VddDocumentReviewer* m_vddDocumentReviewer;
    QWidget* m_reviewerPage;
    QWidget* m_homePage;            // Central widget that holds the Home page content
    QTabWidget* m_mainTabWidget;    // Main tab widget with Home / Bulk Checksum / Document Reviewer

    // Action Log Panel
    QDockWidget* m_actionLogDock = nullptr;
    QTextEdit* m_actionLogText = nullptr;
    QPushButton* m_actionLogClearBtn = nullptr;

    // Sleek Local Search Panel
    void updateGuideState();

    // Interactive Guided Tour variables & actions
    int m_tourStep = -1;
    QFrame* m_tourCard = nullptr;
    QLabel* m_tourText = nullptr;
    QPushButton* m_tourNextBtn = nullptr;
    QPushButton* m_tourBackBtn = nullptr;
    QPushButton* m_tourSkipBtn = nullptr;
    void startGuidedTour();
    void showTourStep(int step);
    void endGuidedTour();

    // Action buttons declared as members to manage state programmatically
    QPushButton* m_verifyLocalBtn;
    QPushButton* m_importVddBtn;
    QPushButton* m_exportResultsBtn;

    // Local Options Widgets
    QLineEdit* m_configFileEdit;

    // Status and Progress Widgets
    QProgressBar* m_localProgressBar;
    QLabel* m_statusLabel;

    // LLM query state
    int m_llmImportTargetId;  // next available record id for LLM import
    QString m_llmParseMode;   // "vdd" or "csv" or "xlsx" or "xlx"

    // State Collections
    QMap<int, VddRecord> m_records;
    QMap<int, QThread*> m_verifierThreads;

    // Trackers
    int m_activeHashJobs;
    int m_completedHashJobs;
    int m_totalHashFiles; // Total files to hash (excludes MISSING records)

    // QTimer-based throttling for progress bar updates
    QTimer* m_progressTimer;                // Delays progress bar updates out of the signal storm
    QMap<int, int> m_fileProgressMap;       // Latest per-file progress (id -> 0-100)
    QMutex m_progressMutex;                 // Protects m_fileProgressMap from data races

    // Network Management
    QNetworkAccessManager* m_networkManager;

    // CSV/XLSX data sources
    QStringList m_csvFilePaths;

    // Throttled table update timer
    QTimer* m_tableUpdateTimer = nullptr;

    // Collapsible row management
    QMap<int, int> m_mainRowToDetailRow;  // Maps main row to detail row
    QMap<int, bool> m_rowExpanded;        // Tracks which rows are expanded

    // Loading dialog for VDD import
    QPointer<QProgressDialog> m_loadingDialog;

    void setFileProgress(int recordId, int progress);
    void getOverallProgress(int& totalFiles, double& totalProgress);
    void clearActionLog();
    void onTableUpdateFlush();
    void createActionLogDock();

    // File parsing helpers
    QStringList parseCSV(const QString& filePath);
    QStringList parseCSVLine(const QString& line, QChar delimiter = ',');
    QStringList parseXLSX(const QString& filePath);
    int excelColToIndex(const QString& colStr);
};

#endif // MAINWINDOW_H

