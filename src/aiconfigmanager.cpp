#include "aiconfigmanager.h"
#include <QMutexLocker>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QSslConfiguration>
#include <QSslSocket>

AIConfigManager* AIConfigManager::m_instance = nullptr;
QMutex AIConfigManager::m_mutex;

AIConfigManager* AIConfigManager::getInstance() {
    QMutexLocker locker(&m_mutex);
    if (!m_instance) {
        m_instance = new AIConfigManager();
    }
    return m_instance;
}

AIConfigManager::AIConfigManager(QObject* parent)
    : QObject(parent)
{
    m_settings = new QSettings("DevCorp", "VDDAuditSystem", this);
    m_networkManager = new QNetworkAccessManager(this);

    // Configure TLS to support older servers (internal gateways often use outdated configs)
    QSslConfiguration defConfig = QSslConfiguration::defaultConfiguration();
    defConfig.setProtocol(QSsl::TlsV1_0OrLater);
    QSslConfiguration::setDefaultConfiguration(defConfig);

    m_modelRequestReply = nullptr;
    m_isQueryRequest = false;
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &AIConfigManager::onFinished);
}

AIConfigManager::~AIConfigManager() {
    // Relying on QObject parent-child hierarchy for deletion
}

// Returns true if the URL points to Google's Generative Language (Gemini) API
static bool isGeminiUrl(const QString& url) {
    return url.contains("googleapis.com") || url.contains("generativelanguage");
}

// Returns true if the URL points to Anthropic's API directly
static bool isAnthropicUrl(const QString& url) {
    return url.contains("anthropic.com");
}

// Builds the Gemini generateContent URL with the selected model substituted in.
// Handles both full URLs (with /models/...:generateContent) and base URLs.
static QString buildGeminiQueryUrl(const QString& gatewayUrl, const QString& model) {
    int modelsIdx = gatewayUrl.indexOf("/models/");
    if (modelsIdx >= 0) {
        // Replace whatever model name is in the URL with the selected model
        QString base = gatewayUrl.left(modelsIdx); // e.g. .../v1beta
        return base + "/models/" + model + ":generateContent";
    }
    // Base URL only — append model path
    QString base = gatewayUrl;
    if (base.endsWith('/')) base.chop(1);
    return base + "/models/" + model + ":generateContent";
}

QString AIConfigManager::getGatewayUrl() const {
    QMutexLocker locker(&m_mutex);
    return m_settings->value("gatewayUrl", "https://api.anthropic.com/v1").toString();
}

void AIConfigManager::setGatewayUrl(const QString& url) {
    QMutexLocker locker(&m_mutex);
    m_settings->setValue("gatewayUrl", url);
}

QString AIConfigManager::getAccessToken() const {
    QMutexLocker locker(&m_mutex);
    return m_settings->value("accessToken", "").toString();
}

void AIConfigManager::setAccessToken(const QString& token) {
    QMutexLocker locker(&m_mutex);
    m_settings->setValue("accessToken", token);
}

double AIConfigManager::getTemperature() const {
    QMutexLocker locker(&m_mutex);
    return m_settings->value("temperature", 0.0).toDouble();
}

void AIConfigManager::setTemperature(double temp) {
    QMutexLocker locker(&m_mutex);
    m_settings->setValue("temperature", temp);
}

int AIConfigManager::getMaxTokens() const {
    QMutexLocker locker(&m_mutex);
    return m_settings->value("maxTokens", 4096).toInt();
}

void AIConfigManager::setMaxTokens(int limit) {
    QMutexLocker locker(&m_mutex);
    m_settings->setValue("maxTokens", limit);
}

QString AIConfigManager::getTargetModel() const {
    QMutexLocker locker(&m_mutex);
    return m_settings->value("targetModel", "claude-3-5-sonnet-20241022").toString();
}

void AIConfigManager::setTargetModel(const QString& model) {
    QMutexLocker locker(&m_mutex);
    m_settings->setValue("targetModel", model);
}

void AIConfigManager::discoverModels() {
    QString gw = getGatewayUrl();
    if (gw.endsWith('/')) gw.chop(1);

    if (isGeminiUrl(gw)) {
        // Gemini model list is static — dynamic discovery requires a separate key-authenticated endpoint
        QStringList geminiModels;
        geminiModels << "gemini-2.5-flash"
                     << "gemini-2.0-flash"
                     << "gemini-2.0-flash-lite"
                     << "gemini-1.5-flash"
                     << "gemini-1.5-flash-8b"
                     << "gemini-1.5-pro"
                     << "gemini-flash-latest";
        emit modelsDiscovered(geminiModels);
        return;
    }

    if (gw.contains("anthropic.com")) {
        QStringList fixedAnthropicModels;
        fixedAnthropicModels << "claude-3-5-sonnet-20241022"
                             << "claude-3-opus-20240229"
                             << "claude-3-haiku-20240307"
                             << "claude-3-5-sonnet-20240229:latest"
                             << "claude-3-opus-20240229:latest";
        emit modelsDiscovered(fixedAnthropicModels);
        return;
    }

    // For other gateways, try OpenAI-compatible /v1/models endpoint
    QString urlStr = gw + "/v1/models";
    QUrl url(urlStr);
    QNetworkRequest request(url);

    QSslConfiguration reqSsl = QSslConfiguration::defaultConfiguration();
    reqSsl.setProtocol(QSsl::TlsV1_0OrLater);
    request.setSslConfiguration(reqSsl);

    QString token = getAccessToken();
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    }
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    m_modelRequestReply = m_networkManager->get(request);
}

void AIConfigManager::onFinished(QNetworkReply* reply)
{
    QUrl requestUrl = reply->url();
    QString requestPath = requestUrl.path();

    // Distinguish model-discovery replies from query replies.
    // Gemini query URLs contain ":generateContent" in the path — do NOT treat them as model discovery
    // even though they also contain "/models/".
    bool isModelDiscovery = requestPath.contains("/models")
                            && !requestPath.contains(":generateContent")
                            && !m_isQueryRequest;
    m_isQueryRequest = false;

    // --- Model discovery ---
    if (isModelDiscovery) {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QString errorMsg = QString("Network error (%1): %2").arg(reply->error()).arg(reply->errorString());
            emit discoveryFailed(errorMsg);
            return;
        }

        QByteArray data = reply->readAll();
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError) {
            emit discoveryFailed("Failed to parse dynamic models listing response JSON: " + error.errorString());
            return;
        }

        QStringList discoveredModels;
        if (doc.isObject()) {
            QJsonObject root = doc.object();
            if (root.contains("data") && root["data"].isArray()) {
                QJsonArray arr = root["data"].toArray();
                for (const QJsonValue& val : arr) {
                    if (val.isObject() && val.toObject().contains("id")) {
                        discoveredModels.append(val.toObject()["id"].toString());
                    }
                }
            }
        } else if (doc.isArray()) {
            QJsonArray arr = doc.array();
            for (const QJsonValue& val : arr) {
                if (val.isString()) {
                    discoveredModels.append(val.toString());
                }
            }
        }

        if (!discoveredModels.isEmpty()) {
            emit modelsDiscovered(discoveredModels);
        } else {
            emit discoveryFailed("Parsed reply but no valid model objects with 'id' parameters were found.");
        }
        return;
    }

    // --- LLM text query ---
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit queryFailed(reply->errorString(), m_currentQuerySourceId);
        return;
    }

    QByteArray data = reply->readAll();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        emit queryFailed("Failed to parse LLM response JSON: " + error.errorString(), m_currentQuerySourceId);
        return;
    }

    QString result;

    if (doc.isObject()) {
        QJsonObject root = doc.object();

        // Gemini: {"candidates":[{"content":{"parts":[{"text":"..."}],"role":"model"}}]}
        if (root.contains("candidates") && root["candidates"].isArray()) {
            QJsonArray candidates = root["candidates"].toArray();
            if (!candidates.isEmpty() && candidates.first().isObject()) {
                QJsonObject candidate = candidates.first().toObject();
                if (candidate.contains("content") && candidate["content"].isObject()) {
                    QJsonObject content = candidate["content"].toObject();
                    if (content.contains("parts") && content["parts"].isArray()) {
                        QJsonArray parts = content["parts"].toArray();
                        if (!parts.isEmpty() && parts.first().isObject()) {
                            result = parts.first().toObject()["text"].toString();
                        }
                    }
                }
            }
            if (!result.isEmpty()) {
                emit queryResult(result, m_currentQuerySourceId);
                return;
            }
        }

        // Anthropic v1/messages: {"content": [{"type":"text","text":"..."}, ...]}
        if (root.contains("content") && root["content"].isArray()) {
            QJsonArray arr = root["content"].toArray();
            for (const QJsonValue& val : arr) {
                if (val.isObject()) {
                    QString type = val.toObject()["type"].toString();
                    if (type == "text") {
                        result = val.toObject()["text"].toString();
                        break;
                    } else if (type == "tool_use" && val.toObject().contains("input")) {
                        result = QJsonDocument(val.toObject()["input"].toObject()).toJson(QJsonDocument::Compact);
                        break;
                    }
                } else if (val.isString()) {
                    result = val.toString();
                    break;
                }
            }
            if (!result.isEmpty()) {
                emit queryResult(result, m_currentQuerySourceId);
                return;
            }
        }

        // OpenAI-compatible: {"choices":[{"message":{"content":"..."}}]}
        if (root.contains("choices") && root["choices"].isArray()) {
            QJsonArray choices = root["choices"].toArray();
            if (!choices.isEmpty()) {
                QJsonObject msg = choices.first().toObject();
                if (msg.contains("message") && msg["message"].isObject()) {
                    result = msg["message"].toObject()["content"].toString();
                }
                if (result.isEmpty()) {
                    result = msg["content"].toString();
                }
                if (!result.isEmpty()) {
                    emit queryResult(result, m_currentQuerySourceId);
                    return;
                }
            }
        }

        // Direct text response
        if (result.isEmpty() && root.contains("text")) {
            result = root["text"].toString();
            if (!result.isEmpty()) {
                emit queryResult(result, m_currentQuerySourceId);
                return;
            }
        }

        // Fallback: raw JSON as string
        emit queryResult(QString::fromUtf8(data), m_currentQuerySourceId);
        return;
    } else if (doc.isArray()) {
        emit queryResult(QString::fromUtf8(data), m_currentQuerySourceId);
    } else {
        emit queryFailed("Unexpected LLM response format.", m_currentQuerySourceId);
    }
}

// --- LLM text query ---

void AIConfigManager::queryText(const QString& promptText, const QString& systemPrompt, int querySourceId)
{
    if (getAccessToken().isEmpty()) {
        emit queryFailed("No API access token configured.", querySourceId);
        return;
    }

    m_currentModel = getTargetModel();
    m_currentMaxTokens = getMaxTokens();
    m_currentTemperature = getTemperature();
    m_currentQuerySourceId = querySourceId;

    QString gw = getGatewayUrl();
    if (gw.endsWith('/')) gw.chop(1);

    m_isQueryRequest = true;
    emit queryStarted(querySourceId);
    emit queryProgress("Request sent, waiting for response...", querySourceId);

    QNetworkRequest request;
    QSslConfiguration reqSsl = QSslConfiguration::defaultConfiguration();
    reqSsl.setProtocol(QSsl::TlsV1_0OrLater);
    request.setSslConfiguration(reqSsl);
    request.setRawHeader("Content-Type", "application/json");

    QJsonDocument doc;

    if (isGeminiUrl(gw)) {
        // Gemini: build URL dynamically so the selected model is always used
        QString queryUrl = buildGeminiQueryUrl(gw, m_currentModel);
        request.setUrl(QUrl(queryUrl));
        request.setRawHeader("X-goog-api-key", getAccessToken().toUtf8());

        QJsonObject json;

        // System instruction (Gemini native format)
        if (!systemPrompt.isEmpty()) {
            QJsonObject sysInstruction;
            QJsonArray sysParts;
            QJsonObject sysPart;
            sysPart["text"] = systemPrompt;
            sysParts.append(sysPart);
            sysInstruction["parts"] = sysParts;
            json["system_instruction"] = sysInstruction;
        }

        // User message
        QJsonArray contents;
        QJsonObject userContent;
        QJsonArray parts;
        QJsonObject part;
        part["text"] = promptText;
        parts.append(part);
        userContent["role"] = "user";
        userContent["parts"] = parts;
        contents.append(userContent);
        json["contents"] = contents;

        // Generation config
        QJsonObject genConfig;
        genConfig["maxOutputTokens"] = m_currentMaxTokens;
        genConfig["temperature"] = m_currentTemperature;
        json["generationConfig"] = genConfig;

        doc = QJsonDocument(json);
    } else if (isAnthropicUrl(gw)) {
        // Anthropic direct API
        request.setUrl(QUrl(gw + "/v1/messages"));
        request.setRawHeader("x-api-key", getAccessToken().toUtf8());
        request.setRawHeader("anthropic-version", "2023-06-01");

        QJsonObject json;
        json["model"] = m_currentModel;
        json["max_tokens"] = m_currentMaxTokens;
        json["temperature"] = m_currentTemperature;

        if (!systemPrompt.isEmpty()) {
            json["system"] = systemPrompt;
        }

        QJsonArray messages;
        QJsonObject userMsg;
        userMsg["role"] = "user";
        userMsg["content"] = promptText;
        messages.append(userMsg);
        json["messages"] = messages;

        doc = QJsonDocument(json);
    } else {
        // OpenAI-compatible gateway (OpenRouter, Ollama, Azure, internal proxies, etc.)
        // Use /v1/chat/completions with Authorization: Bearer header
        QString baseUrl = gw;
        // If URL already ends with /v1/chat/completions, use as-is; otherwise append
        if (!baseUrl.endsWith("/v1/chat/completions")) {
            if (baseUrl.endsWith("/v1")) {
                baseUrl += "/chat/completions";
            } else {
                baseUrl += "/v1/chat/completions";
            }
        }
        request.setUrl(QUrl(baseUrl));
        request.setRawHeader("Authorization", ("Bearer " + getAccessToken()).toUtf8());

        QJsonObject json;
        json["model"] = m_currentModel;
        json["max_tokens"] = m_currentMaxTokens;
        json["temperature"] = m_currentTemperature;

        QJsonArray messages;
        if (!systemPrompt.isEmpty()) {
            QJsonObject sysMsg;
            sysMsg["role"] = "system";
            sysMsg["content"] = systemPrompt;
            messages.append(sysMsg);
        }
        QJsonObject userMsg;
        userMsg["role"] = "user";
        userMsg["content"] = promptText;
        messages.append(userMsg);
        json["messages"] = messages;

        doc = QJsonDocument(json);
    }

    m_networkManager->post(request, doc.toJson());
}
