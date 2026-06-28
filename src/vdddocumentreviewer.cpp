#include "vdddocumentreviewer.h"
#include "aiconfigmanager.h"
#include "odtextractor.h"

#include "mainwindow.h"

#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QMessageBox>
#include <QRegularExpression>
#include <QApplication>

static const char* SYSTEM_PROMPT =
    "You are a senior quality assurance reviewer specializing in Version Description Documents (VDDs), which are technical documents used in embedded systems to list Configuration Items (CIs), their versions, and file integrity checksums.\n\n"
    "You will receive TWO data sources:\n"
    "A) RAW DOCUMENT TEXT - the plain-text content extracted from the VDD file.\n"
    "B) STRUCTURED EXTRACTED RECORDS - CI data previously parsed from the same document (or from a companion config file).\n\n"
    "Your job is to compare these two sources and provide an accurate, evidence-based review.\n\n"
    "CRITICAL GUIDELINES:\n"
    "1. When validating document structure, reference specific sections from the RAW DOCUMENT TEXT.\n"
    "2. When validating CI items, CHECK THE RAW DOCUMENT TEXT directly. Verify that each CI listed in the STRUCTURED RECORDS actually appears in the document text with matching CI reference, version, and checksum.\n"
    "3. For cross-reference comparison:\n"
    "   - Compare the STRUCTURED RECORDS against the RAW DOCUMENT TEXT line-by-line.\n"
    "   - Report which CI items from the records are actually found in the document.\n"
    "   - Report which CI items from the document appear to be missing from the structured records.\n"
    "   - Flag any discrepancies in CI references, versions, or checksums between the two sources.\n"
    "4. For checksum validation:\n"
    "   - MD5 hashes should be exactly 32 hexadecimal characters.\n"
    "   - CRC-32 hashes should be exactly 8 hexadecimal characters.\n"
    "   - Cross-check checksums between the raw document text and the structured records.\n"
    "5. CONFIG-ONLY items (marked [CONFIG-ONLY]) are CI references found in a configuration file but NOT in the VDD document. These are informational. Note whether they should potentially be added to the VDD.\n\n"
    "OUTPUT FORMAT:\n"
    "Use this structure for your review:\n\n"
    "## Document Overview\n"
    "Brief summary of the VDD document (project, version, date).\n\n"
    "## Structure Validation\n"
    "Check if the document has all expected sections: scope, CI tables, revision history.\n\n"
    "## CI Item Cross-Reference\n"
    "For each CI item, state whether it was found in the document text with matching reference, version, and checksum.\n\n"
    "## Checksum Validation\n"
    "Verify MD5 and CRC-32 formats and cross-check values.\n\n"
    "## Missing Items\n"
    "List any CIs present in records but missing from the document, and vice versa.\n\n"
    "## Summary\n"
    "Overall assessment: number of items verified, any issues found, confidence level.\n\n"
    "Be precise and factual. Only flag issues you can verify. If data looks correct, say so. Avoid generic statements - cite specific CI references and values.";

VddDocumentReviewer::VddDocumentReviewer(QWidget *parent)
    : QWidget(parent)
{
    buildUi();

    connect(AIConfigManager::getInstance(), &AIConfigManager::queryStarted,
            this, &VddDocumentReviewer::onLLMQueryStarted);
    connect(AIConfigManager::getInstance(), &AIConfigManager::queryProgress,
            this, &VddDocumentReviewer::onLLMQueryProgress);
    connect(AIConfigManager::getInstance(), &AIConfigManager::queryResult,
            this, &VddDocumentReviewer::onLLMQueryResult);
    connect(AIConfigManager::getInstance(), &AIConfigManager::queryFailed,
            this, &VddDocumentReviewer::onLLMQueryFailed);
}

void VddDocumentReviewer::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 8, 12, 8);
    mainLayout->setSpacing(8);

    // Header
    auto* headerLayout = new QHBoxLayout();
    auto* titleLabel = new QLabel("Document Review", this);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #6366F1;");
    auto* subtitleLabel = new QLabel("AI-powered VDD quality analysis and CI cross-reference audit", this);
    subtitleLabel->setStyleSheet("color: #6B7280; font-size: 11px;");
    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(subtitleLabel, 1);
    mainLayout->addLayout(headerLayout);

    // Path selector
    auto* pathLayout = new QHBoxLayout();
    m_odtPathEdit = new QLineEdit(this);
    m_odtPathEdit->setPlaceholderText("No ODT file selected — click Browse to select one");
    m_odtPathEdit->setReadOnly(true);
    pathLayout->addWidget(m_odtPathEdit, 1);
    m_btnBrowseOdt = new QPushButton("Browse ODT", this);
    connect(m_btnBrowseOdt, &QPushButton::clicked, this, &VddDocumentReviewer::onBrowseOdtClicked);
    pathLayout->addWidget(m_btnBrowseOdt);
    mainLayout->addLayout(pathLayout);

    // Action buttons
    auto* btnLayout = new QHBoxLayout();
    m_btnRunReview = new QPushButton("Run Review", this);
    m_btnRunReview->setObjectName("runReviewBtn");
    connect(m_btnRunReview, &QPushButton::clicked, this, &VddDocumentReviewer::onRunReview);
    btnLayout->addWidget(m_btnRunReview);

    m_btnClear = new QPushButton("Clear Results", this);
    m_btnClear->setObjectName("clearReviewBtn");
    connect(m_btnClear, &QPushButton::clicked, this, &VddDocumentReviewer::onClearResults);
    btnLayout->addWidget(m_btnClear);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    // Content area: LLM Review Panel
    m_llmReviewPanel = new QTextEdit(this);
    m_llmReviewPanel->setObjectName("llmReviewPanel");
    m_llmReviewPanel->setReadOnly(true);
    m_llmReviewPanel->setWordWrapMode(QTextOption::WordWrap);
    m_llmReviewPanel->setStyleSheet(
        "QTextEdit { background: rgba(15, 23, 42, 0.95); color: #E2E8F0; "
        "border: 1px solid rgba(99, 102, 241, 0.2); "
        "border-radius: 8px; padding: 12px; font-size: 12px; }");
    m_llmReviewPanel->setPlaceholderText(
        "No review results yet.\n\n"
        "How to use:\n"
        "1. Import a VDD document (ODT) on the Home tab first\n"
        "2. Or click 'Browse ODT' above to select a document directly\n"
        "3. Click 'Run Review' to start AI-powered quality analysis\n\n"
        "The review will check:\n"
        "  - Document structure and completeness\n"
        "  - CI reference accuracy and consistency\n"
        "  - Checksum format validation (MD5, CRC-32)\n"
        "  - Cross-reference between document text and extracted records");
    mainLayout->addWidget(m_llmReviewPanel, 1);

    // Footer: progress + status
    auto* footerLayout = new QHBoxLayout();
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);
    // Don't show indeterminate progress at startup — it looks stuck
    m_progressBar->hide();
    m_progressBar->setStyleSheet(
        "QProgressBar { background: rgba(229, 231, 235, 0.8); border: 1px solid rgba(99, 102, 241, 0.2); "
        "border-radius: 4px; text-align: center; color: #6B7280; }"
        "QProgressBar::chunk { background: #6366F1; }");
    footerLayout->addWidget(m_progressBar, 1);
    m_statusLabel = new QLabel("Ready", this);
    m_statusLabel->setStyleSheet("color: #6B7280; font-size: 11px; font-style: italic;");
    footerLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(footerLayout);
}

void VddDocumentReviewer::setExistingRecords(const QMap<int, VddRecord>& records)
{
    m_existingRecords = records;
}

void VddDocumentReviewer::setOdtFilePath(const QString& path)
{
    m_odtFilePath = path;
    m_odtPathEdit->setText(path);
}

void VddDocumentReviewer::onBrowseOdtClicked()
{
    QString path = QFileDialog::getOpenFileName(this, "Select VDD Document (ODT)", "", "OpenDocument Text (*.odt)");
    if (path.isEmpty()) return;
    setOdtFilePath(path);
}

void VddDocumentReviewer::onRunReview()
{
    if (m_busy) {
        QMessageBox::warning(this, "Review In Progress", "A review is already running. Please wait or clear results.");
        return;
    }

    QString odtPath = m_odtFilePath;
    if (odtPath.isEmpty()) {
        QMessageBox::information(this, "No File Selected", "Please select an ODT file first.");
        return;
    }

    m_statusLabel->setText("Validating ODT file...");
    QApplication::processEvents(QEventLoop::AllEvents, 5000);

    if (!OdtExtractor::isValidOdt(odtPath)) {
        QMessageBox::critical(this, "Invalid ODT", "Cannot read OpenDocument container. The file might be corrupted.");
        m_statusLabel->setText("Review failed: invalid ODT file.");
        return;
    }

    QString plain = OdtExtractor::extractPlainText(odtPath);
    if (plain.isEmpty()) {
        QMessageBox::critical(this, "Extraction Failed", "content.xml is missing from the archive.");
        m_statusLabel->setText("Review failed: missing content.xml.");
        return;
    }

    m_extractedText = plain;

    // Build the LLM prompt
    QString userPrompt = buildReviewPrompt();

    // Start the review query
    ++m_queryToken;
    setBusy(true);
    m_llmReviewPanel->clear();
    m_progressBar->show();
    m_progressBar->setRange(0, 0); // indeterminate
    m_statusLabel->setText("Sending review request...");

    emit reviewStarted();

    AIConfigManager::getInstance()->queryText(userPrompt, SYSTEM_PROMPT, m_queryToken);
}

QString VddDocumentReviewer::buildReviewPrompt() const
{
    QString prompt;

    // === SECTION 1: Raw document text (what's actually in the file) ===
    prompt += "SOURCE A: RAW VDD DOCUMENT TEXT\n";
    prompt += "This is the plain text extracted from the VDD document file.\n\n";
    prompt += "```\n" + m_extractedText.trimmed() + "\n```\n\n";

    // === SECTION 2: Structured extracted records ===
    prompt += "SOURCE B: STRUCTURED EXTRACTED RECORDS\n";
    prompt += "These are the CI records that were previously parsed from the document or companion config files.\n\n";
    if (!m_existingRecords.isEmpty()) {
        for (const VddRecord& rec : m_existingRecords) {
            QString prefix = (rec.source == "CONFIG_ONLY") ? "[CONFIG-ONLY] " : "[VDD] ";
            if (rec.source == "CONFIG_ONLY") {
                prompt += QString("  %1 ID:%2 | File: %3 | CI: %4 | ConfigVer: %5 | Path: %6 | PathExists: %7 | FileFound: %8 | ExpectedMD5: %9 | LocalStatus: %10\n")
                              .arg(prefix)
                              .arg(rec.id)
                              .arg(rec.fileName)
                              .arg(rec.ciReference.isEmpty() ? "(none)" : rec.ciReference)
                              .arg(rec.configVersion.isEmpty() ? "(none)" : rec.configVersion)
                              .arg(rec.configPath.isEmpty() ? "(none)" : rec.configPath)
                              .arg(rec.configPathExists ? "Yes" : "No")
                              .arg(rec.configFileFoundAtPath ? "Yes" : "No")
                              .arg(rec.expectedMd5.isEmpty() ? "(none)" : rec.expectedMd5)
                              .arg(rec.localStatus);
            } else {
                prompt += QString("  %1 ID:%2 | File: %3 | CI: %4 | Version: %5 | ExpectedMD5: %6 | ExpectedCRC32: %7 | LocalStatus: %8\n")
                              .arg(prefix)
                              .arg(rec.id)
                              .arg(rec.fileName)
                              .arg(rec.ciReference.isEmpty() ? "(none)" : rec.ciReference)
                              .arg(rec.version.isEmpty() ? "(none)" : rec.version)
                              .arg(rec.expectedMd5.isEmpty() ? "(none)" : rec.expectedMd5)
                              .arg(rec.expectedCrc32.isEmpty() ? "(none)" : rec.expectedCrc32)
                              .arg(rec.localStatus);
            }
        }
    } else {
        prompt += "  No structured records available.\n";
    }

    prompt += "\n---\n";
    prompt += "TASK: Compare SOURCE A (the raw document) against SOURCE B (the parsed records). Identify any mismatches, missing items, or discrepancies. Provide a structured review.\n";

    return prompt;
}

void VddDocumentReviewer::displayReviewResult(const QString& llmResponse)
{
    // Convert markdown-style LLM response to HTML for rich display
    QString html = llmResponse;

    // Escape HTML entities first
    html.replace("&", "&amp;");
    html.replace("<", "&lt;");
    html.replace(">", "&gt;");

    // Convert markdown headers: ## Header → <b>Header</b><br>
    html.replace(QRegularExpression("^### (.+)$", QRegularExpression::MultilineOption),
                 "<b style='color:#6366F1;font-size:13px;'>\\1</b><br>");
    html.replace(QRegularExpression("^## (.+)$", QRegularExpression::MultilineOption),
                 "<b style='color:#6366F1;font-size:14px;'>\\1</b><br>");
    html.replace(QRegularExpression("^# (.+)$", QRegularExpression::MultilineOption),
                 "<b style='color:#6366F1;font-size:15px;'>\\1</b><br>");

    // Convert **bold** → <b>
    html.replace(QRegularExpression("\\*\\*(.+?)\\*\\*"), "<b>\\1</b>");

    // Convert `code` → monospace
    html.replace(QRegularExpression("`([^`]+)`"), "<span style='font-family:Consolas,monospace;background:#1e293b;padding:1px 4px;border-radius:3px;'>\\1</span>");

    // Convert ```code blocks```
    html.replace(QRegularExpression("```\\w*\\n"), "<pre style='background:#0f172a;border:1px solid #334155;padding:8px;border-radius:4px;font-family:Consolas,monospace;font-size:11px;overflow-x:auto;'>");
    html.replace("```", "</pre>");

    // Convert line breaks
    html.replace("\n", "<br>");

    // Convert bullet points: "- item" or "* item"
    html.replace(QRegularExpression("^(-|\\*) (.+)$", QRegularExpression::MultilineOption),
                 "&nbsp;&nbsp;&nbsp;• \\2");

    m_llmReviewPanel->setHtml(html);
    m_llmReviewPanel->moveCursor(QTextCursor::Start);
    emit reviewCompleted(llmResponse);
}

void VddDocumentReviewer::setBusy(bool busy)
{
    m_busy = busy;
    m_btnRunReview->setEnabled(!busy);
    m_btnBrowseOdt->setEnabled(!busy);
    if (busy) {
        m_progressBar->show();
        m_progressBar->setRange(0, 0); // indeterminate
    } else {
        m_progressBar->hide();
    }
}

void VddDocumentReviewer::startReview()
{
    if (m_odtFilePath.isEmpty()) {
        QMessageBox::information(this, "No File Selected", "Please select an ODT file first (click Browse).");
        return;
    }
    onRunReview();
}

void VddDocumentReviewer::cancelReview()
{
    if (!m_busy) return;
    ++m_queryToken; // Invalidate in-flight query results
    setBusy(false);
    m_statusLabel->setText("Review cancelled.");
}

void VddDocumentReviewer::onClearResults()
{
    m_llmReviewPanel->clear();
    m_extractedText.clear();
    m_progressBar->hide();
    m_statusLabel->setText("Results cleared.");
    setBusy(false);
}

void VddDocumentReviewer::onLLMQueryStarted(int sourceId)
{
    if (sourceId != m_queryToken) return;
    m_statusLabel->setText("Review request sent, waiting for response...");
    emit reviewProgress("Request sent, waiting for response...");
}

void VddDocumentReviewer::onLLMQueryProgress(const QString& status, int sourceId)
{
    if (sourceId != m_queryToken) return;
    m_statusLabel->setText(status);
}

void VddDocumentReviewer::onLLMQueryResult(const QString& result, int sourceId)
{
    if (sourceId != m_queryToken) return;
    displayReviewResult(result);
    setBusy(false);
    m_statusLabel->setText("Review completed.");
}

void VddDocumentReviewer::onLLMQueryFailed(const QString& errMsg, int sourceId)
{
    if (sourceId != m_queryToken) return;
    setBusy(false);
    m_statusLabel->setText("Review failed: " + errMsg);
    QMessageBox::warning(this, "Review Failed", "Document review failed: " + errMsg);
}
