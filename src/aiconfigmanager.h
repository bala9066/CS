#ifndef AICONFIGMANAGER_H
#define AICONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSettings>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class AIConfigManager : public QObject {
    Q_OBJECT
public:
    static AIConfigManager* getInstance();

    // Persistent Properties
    QString getGatewayUrl() const;
    void setGatewayUrl(const QString& url);

    QString getAccessToken() const;
    void setAccessToken(const QString& token);

    double getTemperature() const;
    void setTemperature(double temp);

    int getMaxTokens() const;
    void setMaxTokens(int limit);

    QString getTargetModel() const;
    void setTargetModel(const QString& model);

    // Asynchronous dynamic model discovery
    void discoverModels();

    // LLM text query for data extraction
    // querySourceId distinguishes concurrent queries from different UI components
    void queryText(const QString& promptText, const QString& systemPrompt, int querySourceId = 0);

signals:
    void queryStarted(int sourceId);
    void queryProgress(const QString& status, int sourceId);
    void queryResult(const QString& result, int sourceId);
    void queryFailed(const QString& errorMsg, int sourceId);
    void modelsDiscovered(const QStringList& models);
    void discoveryFailed(const QString& errorMsg);

private slots:
    void onFinished(QNetworkReply* reply);

private:
    explicit AIConfigManager(QObject* parent = nullptr);
    ~AIConfigManager();
    AIConfigManager(const AIConfigManager&) = delete;
    AIConfigManager& operator=(const AIConfigManager&) = delete;

    static AIConfigManager* m_instance;
    static QMutex m_mutex;

    QSettings* m_settings;
    QNetworkAccessManager* m_networkManager;

    // LLM query support
    QString m_currentModel;
    int m_currentMaxTokens;
    double m_currentTemperature;

    // Request tracking to distinguish callbacks
    QNetworkReply* m_modelRequestReply = nullptr;
    bool m_isQueryRequest = false;
    int m_currentQuerySourceId = 0;
};

#endif // AICONFIGMANAGER_H
