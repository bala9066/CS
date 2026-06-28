#include "bulkchecksumdock.h"
#include "checksumworker.h"
#include "folderscanner.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QDialogButtonBox>

BulkChecksumDock::BulkChecksumDock(QWidget *parent)
    : QWidget(parent)
    , m_idCounter(0)
    , m_activeJobs(0)
    , m_completedJobs(0)
{
    setObjectName("bulkChecksumDock");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // Header
    QLabel* titleLabel = new QLabel("Multi-File Hash Calculator", this);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #4F46E5;");
    mainLayout->addWidget(titleLabel);

    QLabel* subtitleLabel = new QLabel(
        "Drag and drop files below, or select files manually to process MD5, SHA-1 and CRC32 "
        "in background threads.", this);
    subtitleLabel->setStyleSheet("font-size: 11px; color: #6B7280; margin-bottom: 6px;");
    mainLayout->addWidget(subtitleLabel);

    // Table
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(7);
    m_tableWidget->setHorizontalHeaderLabels({
        "File Name", "Size", "Status", "Progress", "MD5", "SHA-1", "CRC32"
    });

    m_tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setAlternatingRowColors(true);
    mainLayout->addWidget(m_tableWidget);

    // Progress section
    QHBoxLayout* progressLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("Ready. Load files to begin hashing.", this);
    m_statusLabel->setMinimumWidth(200);
    m_overallProgressBar = new QProgressBar(this);
    m_overallProgressBar->setRange(0, 100);
    m_overallProgressBar->setValue(0);
    m_overallProgressBar->setTextVisible(true);

    progressLayout->addWidget(m_statusLabel);
    progressLayout->addWidget(m_overallProgressBar, 1);
    mainLayout->addLayout(progressLayout);

    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();

    m_btnAddFiles = new QPushButton("Add Files...", this);
    m_btnScanFolder = new QPushButton("Scan Folder...", this);
    m_btnStart = new QPushButton("Start Calculation", this);
    m_btnCancel = new QPushButton("Cancel Active", this);
    m_btnClear = new QPushButton("Clear Completed", this);

    QPushButton* btnCopyHash = new QPushButton("Copy MD5 Hash", this);

    m_btnCancel->setEnabled(false);

    btnLayout->addWidget(m_btnAddFiles);
    btnLayout->addWidget(m_btnScanFolder);
    btnLayout->addWidget(m_btnStart);
    btnLayout->addWidget(m_btnCancel);
    btnLayout->addWidget(m_btnClear);
    btnLayout->addStretch();
    btnLayout->addWidget(btnCopyHash);

    mainLayout->addLayout(btnLayout);
    setLayout(mainLayout);

    setAcceptDrops(true);

    // Connections
    connect(m_btnAddFiles, &QPushButton::clicked, this, &BulkChecksumDock::onAddFiles);
    connect(m_btnScanFolder, &QPushButton::clicked, this, &BulkChecksumDock::onScanFolder);
    connect(m_btnStart, &QPushButton::clicked, this, &BulkChecksumDock::onStartHashing);
    connect(m_btnCancel, &QPushButton::clicked, this, &BulkChecksumDock::onCancelAll);
    connect(m_btnClear, &QPushButton::clicked, this, &BulkChecksumDock::onClearAll);
    connect(btnCopyHash, &QPushButton::clicked, this, &BulkChecksumDock::onCopySelectedHash);
}

BulkChecksumDock::~BulkChecksumDock() {
    onCancelAll();
}

// ---- Dock dragging to accept drops ----

void BulkChecksumDock::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void BulkChecksumDock::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QStringList filePaths;
        for (const QUrl& url : mimeData->urls()) {
            QString path = url.toLocalFile();
            if (!path.isEmpty() && QFileInfo(path).isFile()) {
                filePaths.append(path);
            }
        }
        addFilesToList(filePaths);
        event->acceptProposedAction();
    }
}

// ---- Core logic ----

void BulkChecksumDock::onAddFiles() {
    QStringList paths = QFileDialog::getOpenFileNames(
        this, "Select Files to Calculate Checks", "", "All Files (*)");
    if (!paths.isEmpty()) {
        addFilesToList(paths);
    }
}

void BulkChecksumDock::onScanFolder() {
    QString dirPath = QFileDialog::getExistingDirectory(
        this, "Select Directory to Scan", "",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dirPath.isEmpty()) return;

    // Filter options dialog
    QDialog filterDialog(this);
    filterDialog.setWindowTitle("Scan Folder Filters");
    filterDialog.resize(360, 160);
    QVBoxLayout* dialogLayout = new QVBoxLayout(&filterDialog);

    QLabel* filterInfo = new QLabel("Choose which file types to include:", &filterDialog);
    filterInfo->setWordWrap(true);
    dialogLayout->addWidget(filterInfo);

    QComboBox* presetCombo = new QComboBox(&filterDialog);
    presetCombo->addItems({"All files", "Document files", "Media files",
                           "Archive files", "Custom patterns..."});
    dialogLayout->addWidget(presetCombo);

    QCheckBox* chkDoc = new QCheckBox("Documents: .txt .pdf .doc .xls .csv", &filterDialog);
    QCheckBox* chkMedia = new QCheckBox("Media: .jpg .png .mp3 .mp4 .avi", &filterDialog);
    QCheckBox* chkArchive = new QCheckBox("Archives: .zip .rar .7z .tar .gz", &filterDialog);
    chkDoc->setChecked(true);
    dialogLayout->addWidget(chkDoc);
    dialogLayout->addWidget(chkMedia);
    dialogLayout->addWidget(chkArchive);

    QLabel* customLabel = new QLabel("Custom patterns (semicolon-separated):", &filterDialog);
    customLabel->setStyleSheet("font-size: 10pt; color: #6B7280;");
    customLabel->setWordWrap(true);
    dialogLayout->addWidget(customLabel);

    QLineEdit* customEdit = new QLineEdit(&filterDialog);
    dialogLayout->addWidget(customEdit);

    QDialogButtonBox* btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &filterDialog);
    connect(btnBox, &QDialogButtonBox::accepted, &filterDialog, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &filterDialog, &QDialog::reject);
    dialogLayout->addWidget(btnBox);

    auto applyPreset = [&]() {
        chkDoc->setChecked(false);
        chkMedia->setChecked(false);
        chkArchive->setChecked(false);
        customEdit->clear();
        switch (presetCombo->currentIndex()) {
        case 0: break;
        case 1: chkDoc->setChecked(true); break;
        case 2: chkMedia->setChecked(true); break;
        case 3: chkArchive->setChecked(true); break;
        case 4: customEdit->setFocus(); break;
        }
    };
    connect(presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), applyPreset);
    applyPreset();

    if (filterDialog.exec() != QDialog::Accepted) return;

    QStringList filters;
    if (chkDoc->isChecked()) filters += {"*.txt","*.pdf","*.doc","*.docx","*.xls","*.xlsx","*.csv","*.ppt","*.pptx"};
    if (chkMedia->isChecked()) filters += {"*.jpg","*.jpeg","*.png","*.gif","*.bmp","*.mp3","*.mp4","*.avi","*.wav","*.flac","*.mkv"};
    if (chkArchive->isChecked()) filters += {"*.zip","*.rar","*.7z","*.tar","*.gz","*.bz2","*.xz"};

    QString custom = customEdit->text().trimmed();
    if (!custom.isEmpty()) {
        foreach (const QString& pat, custom.split(';', Qt::SkipEmptyParts)) {
            QString p = pat.trimmed();
            if (!p.isEmpty() && !filters.contains(p)) filters += p;
        }
    }
    if (presetCombo->currentIndex() == 0 && filters.isEmpty()) filters.clear();

    m_btnScanFolder->setEnabled(false);
    m_statusLabel->setText("Scanning folder recursively...");

    QThread* scanThread = new QThread(this);
    FolderScanner* scanner = new FolderScanner(dirPath, filters);
    scanner->moveToThread(scanThread);

    m_pendingScanFiles.clear();

    connect(scanThread, &QThread::started, scanner, &FolderScanner::scan);
    connect(scanner, &FolderScanner::scanProgress, this, [this](int count) {
        m_statusLabel->setText(QString("Scanning... %1 files found.").arg(count));
        QApplication::processEvents(QEventLoop::AllEvents, 5000);
    });
    connect(scanner, &FolderScanner::foundFiles, this, [this](const QStringList& filePaths) {
        m_pendingScanFiles.append(filePaths);
        if (m_pendingScanFiles.size() >= 2000) {
            addFilesToList(m_pendingScanFiles);
            m_pendingScanFiles.clear();
        }
        QApplication::processEvents(QEventLoop::AllEvents, 5000);
    });
    connect(scanner, &FolderScanner::finished, this, [this, scanner, scanThread]() {
        if (!m_pendingScanFiles.isEmpty()) {
            addFilesToList(m_pendingScanFiles);
            m_pendingScanFiles.clear();
        }
        m_btnScanFolder->setEnabled(true);
        if (m_filesMap.isEmpty()) {
            m_statusLabel->setText("No files found in the selected folder.");
        } else {
            m_statusLabel->setText(QString("Imported %1 files from scanning directory.").arg(m_filesMap.size()));
        }
    });
    connect(scanner, &FolderScanner::finished, scanThread, &QThread::quit);
    connect(scanner, &FolderScanner::finished, scanner, &FolderScanner::deleteLater);
    connect(scanThread, &QThread::finished, scanThread, &QThread::deleteLater);

    scanThread->start();
}

void BulkChecksumDock::addFilesToList(const QStringList& paths) {
    for (const QString& path : paths) {
        if (m_pathToRecordId.contains(path)) continue;

        QFileInfo info(path);
        int fileId = m_idCounter++;

        FileInfoRecord record;
        record.id = fileId;
        record.name = info.fileName();
        record.path = path;
        record.size = info.size();
        record.status = "Pending";
        record.progress = 0;

        m_filesMap.insert(fileId, record);
        m_pathToRecordId.insert(path, fileId);

        int row = m_tableWidget->rowCount();
        m_tableWidget->insertRow(row);

        QTableWidgetItem* nameItem = new QTableWidgetItem(record.name);
        nameItem->setToolTip(path);
        m_tableWidget->setItem(row, 0, nameItem);

        if (record.size < 1024) {
            m_tableWidget->setItem(row, 1, new QTableWidgetItem(QString("%1 B").arg(record.size)));
        } else if (record.size < 1024LL * 1024) {
            m_tableWidget->setItem(row, 1, new QTableWidgetItem(QString("%1 KB").arg(record.size / 1024.0, 0, 'f', 2)));
        } else {
            m_tableWidget->setItem(row, 1, new QTableWidgetItem(QString("%1 MB").arg(record.size / (1024.0 * 1024.0), 0, 'f', 2)));
        }
        m_tableWidget->item(row, 1)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        m_tableWidget->setItem(row, 2, new QTableWidgetItem(record.status));

        QProgressBar* cellBar = new QProgressBar(this);
        cellBar->setRange(0, 100);
        cellBar->setValue(0);
        cellBar->setTextVisible(true);
        cellBar->setStyleSheet("QProgressBar { max-height: 14px; text-align: center; }");
        m_tableWidget->setCellWidget(row, 3, cellBar);

        m_tableWidget->setItem(row, 4, new QTableWidgetItem(""));
        m_tableWidget->setItem(row, 5, new QTableWidgetItem(""));
        m_tableWidget->setItem(row, 6, new QTableWidgetItem(""));
    }

    updateOverallProgress();
}

void BulkChecksumDock::onStartHashing() {
    int activeCount = 0;
    for (int fileId : m_filesMap.keys()) {
        FileInfoRecord& rec = m_filesMap[fileId];
        if (rec.status != "Pending") continue;

        QThread* thread = new QThread(this);
        ChecksumWorker* worker = new ChecksumWorker(fileId, rec.path);
        worker->moveToThread(thread);

        rec.thread = thread;
        rec.worker = worker;
        rec.status = "Calculating";

        // Find row by path — build and cache a path-to-row mapping for O(1) lookup.
        int rowIdx = -1;
        for (int r = 0; r < m_tableWidget->rowCount(); ++r) {
            if (m_tableWidget->item(r, 0)->toolTip() == rec.path) {
                rowIdx = r;
                break;
            }
        }
        // Build m_pathToRow lazily once per file
        if (!m_pathToRow.contains(rec.path) && rowIdx != -1) {
            m_pathToRow.insert(rec.path, rowIdx);
        } else if (m_pathToRow.contains(rec.path)) {
            rowIdx = m_pathToRow[rec.path];
        }
        if (rowIdx != -1) {
            m_tableWidget->item(rowIdx, 2)->setText(rec.status);
        }

        connect(thread, &QThread::started, worker, &ChecksumWorker::process);
        connect(worker, &ChecksumWorker::started, this, &BulkChecksumDock::onWorkerStarted);
        connect(worker, &ChecksumWorker::fileProgress, this, &BulkChecksumDock::onWorkerProgress);
        connect(worker, &ChecksumWorker::finished, this, &BulkChecksumDock::onWorkerFinished);

        connect(worker, &ChecksumWorker::finished, thread, &QThread::quit);
        connect(worker, &ChecksumWorker::finished, worker, &ChecksumWorker::deleteLater);
        connect(thread, &QThread::finished, thread, &QThread::deleteLater);

        thread->start();
        activeCount++;
    }

    if (activeCount > 0) {
        m_activeJobs += activeCount;
        m_btnStart->setEnabled(false);
        m_btnAddFiles->setEnabled(false);
        m_btnScanFolder->setEnabled(false);
        m_btnCancel->setEnabled(true);
        m_statusLabel->setText(QString("Running %1 background worker thread(s)...").arg(m_activeJobs));
    }
}

void BulkChecksumDock::onWorkerStarted(int fileId) {
    if (!m_filesMap.contains(fileId)) return;
    m_filesMap[fileId].status = "Active";
}

void BulkChecksumDock::onWorkerProgress(int fileId, int percent) {
    if (!m_filesMap.contains(fileId)) return;

    FileInfoRecord& rec = m_filesMap[fileId];
    rec.progress = percent;

    int rowIdx = -1;
    if (m_pathToRow.contains(rec.path)) {
        rowIdx = m_pathToRow[rec.path];
    } else {
        for (int r = 0; r < m_tableWidget->rowCount(); ++r) {
            if (m_tableWidget->item(r, 0)->toolTip() == rec.path) {
                rowIdx = r;
                break;
            }
        }
        if (rowIdx != -1)
            m_pathToRow.insert(rec.path, rowIdx);
    }
    if (rowIdx != -1) {
        QProgressBar* bar = qobject_cast<QProgressBar*>(m_tableWidget->cellWidget(rowIdx, 3));
        if (bar)
            bar->setValue(percent);
    }
    updateOverallProgress();
}

void BulkChecksumDock::onWorkerFinished(int fileId, const QString& md5, const QString& sha1,
                                        const QString& crc32, bool success, const QString& errorStr) {
    if (!m_filesMap.contains(fileId)) return;

    FileInfoRecord& rec = m_filesMap[fileId];
    rec.md5 = md5;
    rec.sha1 = sha1;
    rec.crc32 = crc32;
    rec.status = success ? "Done" : "Failed";
    rec.thread = nullptr;
    rec.worker = nullptr;

    m_activeJobs--;
    m_completedJobs++;

    // Ensure row lookup is cached
    if (!m_pathToRow.contains(rec.path)) {
        for (int r = 0; r < m_tableWidget->rowCount(); ++r) {
            if (m_tableWidget->item(r, 0)->toolTip() == rec.path) {
                m_pathToRow.insert(rec.path, r);
                break;
            }
        }
    }

    int rowIdx = m_pathToRow.value(rec.path, -1);
    if (rowIdx != -1) {
        m_tableWidget->item(rowIdx, 2)->setText(rec.status);

        QProgressBar* bar = qobject_cast<QProgressBar*>(m_tableWidget->cellWidget(rowIdx, 3));
        if (bar)
            bar->setValue(100);

        if (success) {
            m_tableWidget->item(rowIdx, 4)->setText(md5);
            m_tableWidget->item(rowIdx, 5)->setText(sha1);
            m_tableWidget->item(rowIdx, 6)->setText(crc32);
        } else {
            m_tableWidget->item(rowIdx, 4)->setText(errorStr);
            m_tableWidget->item(rowIdx, 4)->setToolTip(errorStr);
        }
    }

    if (m_activeJobs == 0) {
        m_btnStart->setEnabled(true);
        m_btnAddFiles->setEnabled(true);
        m_btnScanFolder->setEnabled(true);
        m_btnCancel->setEnabled(false);
        m_statusLabel->setText("Thread operations finished. Ready.");
    } else {
        m_statusLabel->setText(QString("Running: %1 | Completed: %2").arg(m_activeJobs).arg(m_completedJobs));
    }

    updateOverallProgress();
}

void BulkChecksumDock::onCancelAll() {
    int cancelledCount = 0;
    for (int fileId : m_filesMap.keys()) {
        FileInfoRecord& rec = m_filesMap[fileId];
        if (rec.worker) {
            ChecksumWorker* workerInstance = qobject_cast<ChecksumWorker*>(rec.worker);
            if (workerInstance) {
                workerInstance->cancel();
                cancelledCount++;
            }
        }
    }
    if (cancelledCount > 0) {
        m_statusLabel->setText("Stopping computing worker threads...");
    }
}

void BulkChecksumDock::onClearAll() {
    QList<int> idsToRemove;
    for (int fileId : m_filesMap.keys()) {
        const FileInfoRecord& rec = m_filesMap[fileId];
        if (rec.status == "Done" || rec.status == "Failed" || rec.status == "Pending") {
            idsToRemove.append(fileId);
        }
    }

    // Remove rows by path using cached mapping for O(1) lookup
    for (int r = m_tableWidget->rowCount() - 1; r >= 0; --r) {
        QString path = m_tableWidget->item(r, 0)->toolTip();
        for (int id : idsToRemove) {
            if (m_filesMap[id].path == path) {
                m_tableWidget->removeRow(r);
                m_pathToRow.remove(path);
                break;
            }
        }
    }

    for (int id : idsToRemove) {
        m_filesMap.remove(id);
    }

    m_completedJobs = 0;
    updateOverallProgress();
}

void BulkChecksumDock::onCopySelectedHash() {
    int row = m_tableWidget->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "Copy Actions", "Please select a valid record first.");
        return;
    }

    QTableWidgetItem* hashItem = m_tableWidget->item(row, 4);
    if (hashItem && !hashItem->text().isEmpty()) {
        QApplication::clipboard()->setText(hashItem->text());
        m_statusLabel->setText("Checksum hash string copied to clipboard.");
    }
}

void BulkChecksumDock::updateOverallProgress() {
    if (m_filesMap.isEmpty()) {
        m_overallProgressBar->setValue(0);
        return;
    }

    int totalProgress = 0;
    for (auto&& rec : m_filesMap.values()) {
        totalProgress += rec.progress;
    }

    int overall = totalProgress / m_filesMap.size();
    m_overallProgressBar->setValue(overall);
}
