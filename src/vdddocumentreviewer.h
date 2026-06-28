#ifndef VDDDOCUMENTREVIEWER_H
#define VDDDOCUMENTREVIEWER_H

#include <QWidget>
#include <QTextEdit>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QMap>

#include "mainwindow.h"

class VddDocumentReviewer : public QWidget {
    Q_OBJECT

public:
    explicit VddDocumentReviewer(QWidget *parent = nullptr);

    void setExistingRecords(const QMap<int, VddRecord>& records);
    void setOdtFilePath(const QString& path);
    QString odtFilePath() const { return m_odtFilePath; }
    void startReview();
    void cancelReview();
    bool isBusy() const { return m_busy; }

signals:
    void reviewStarted();
    void reviewProgress(const QString& status);
    void reviewCompleted(const QString& llmReviewText);
    void reviewFailed(const QString& error);

private slots:
    void onRunReview();
    void onClearResults();
    void onBrowseOdtClicked();
    void onLLMQueryStarted(int sourceId);
    void onLLMQueryProgress(const QString& status, int sourceId);
    void onLLMQueryResult(const QString& result, int sourceId);
    void onLLMQueryFailed(const QString& errMsg, int sourceId);

private:
    void buildUi();
    void displayReviewResult(const QString& llmResponse);
    void setBusy(bool busy);
    QString buildReviewPrompt() const;

    // UI widgets
    QLineEdit* m_odtPathEdit;
    QPushButton* m_btnBrowseOdt;
    QPushButton* m_btnRunReview;
    QPushButton* m_btnClear;
    QTextEdit* m_llmReviewPanel;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;

    // State
    QString m_odtFilePath;
    QMap<int, VddRecord> m_existingRecords;
    QString m_extractedText;
    int m_queryToken = 0;
    bool m_busy = false;
};

#endif // VDDDOCUMENTREVIEWER_H
