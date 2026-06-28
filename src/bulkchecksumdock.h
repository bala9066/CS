#ifndef BULKCHECKSUMDOCK_H
#define BULKCHECKSUMDOCK_H

#include <QDockWidget>
#include <QTableWidget>
#include <QProgressBar>
#include <QLabel>
#include <QMap>
#include <QHash>
#include <QThread>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QDialog>
#include <QPushButton>

class FileInfoRecord {
public:
    int id = 0;
    QString name;
    QString path;
    qint64 size = 0;
    QString status;
    int progress = 0;
    QString md5;
    QString sha1;
    QString crc32;
    QThread* thread = nullptr;
    QObject* worker = nullptr;
};

class BulkChecksumDock : public QWidget {
    Q_OBJECT

public:
    explicit BulkChecksumDock(QWidget *parent = nullptr);
    ~BulkChecksumDock();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onAddFiles();
    void onScanFolder();
    void onStartHashing();
    void onCancelAll();
    void onClearAll();
    void onCopySelectedHash();

    void onWorkerStarted(int fileId);
    void onWorkerProgress(int fileId, int percent);
    void onWorkerFinished(int fileId, const QString& md5, const QString& sha1,
                          const QString& crc32, bool success, const QString& errorStr);

private:
    void addFilesToList(const QStringList& paths);
    void updateOverallProgress();
    QString formatSize(qint64 bytes);

    QTableWidget* m_tableWidget;
    QHash<QString, int> m_pathToRow;  // Cached O(1) lookup from file path to table row
    QProgressBar* m_overallProgressBar;
    QLabel* m_statusLabel;

    QPushButton* m_btnStart;
    QPushButton* m_btnCancel;
    QPushButton* m_btnClear;
    QPushButton* m_btnAddFiles;
    QPushButton* m_btnScanFolder;

    QMap<int, FileInfoRecord> m_filesMap;
    QHash<QString, int> m_pathToRecordId;  // O(1) lookup from file path to record id
    QStringList m_pendingScanFiles;
    int m_idCounter;
    int m_activeJobs;
    int m_completedJobs;
};

#endif // BULKCHECKSUMDOCK_H
