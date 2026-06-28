#ifndef ICONHELPER_H
#define ICONHELPER_H

#include <QObject>
#include <QString>
#include <QIcon>
#include <QPushButton>
#include <QLabel>

/**
 * @brief The IconHelper class provides centralized icon management
 * and creates icon-enhanced UI components for the VDD Audit System.
 */
class IconHelper : public QObject {
    Q_OBJECT
public:
    explicit IconHelper(QObject* parent = nullptr);
    ~IconHelper() override = default;

    /**
     * @brief Loads an icon from the application resources
     * @param name The icon name (without extension or path)
     * @return QIcon object, returns empty icon if not found
     */
    static QIcon loadIcon(const QString& name);

    /**
     * @brief Creates a button with an icon and text
     * @param iconName The name of the icon to use
     * @param text The button text
     * @return QPushButton* with icon and text
     */
    static QPushButton* createIconButton(const QString& iconName, const QString& text);

    /**
     * @brief Creates a status icon label with appropriate styling
     * @param status The status type ("success", "error", "warning", "info")
     * @return QLabel* with status icon and styling
     */
    static QLabel* createStatusIcon(const QString& status);

    /**
     * @brief Creates a label with just an icon
     * @param iconName The name of the icon
     * @param size The icon size (default 16x16)
     * @return QLabel* containing the icon
     */
    static QLabel* createIconLabel(const QString& iconName, int size = 16);

    /**
     * @brief Gets a fallback icon (colored rectangle) for when icons are missing
     * @param type The type of fallback ("primary", "success", "danger", "warning")
     * @param size The size of the icon
     * @return QPixmap with fallback graphic
     */
    static QPixmap getFallbackIcon(const QString& type, int size = 16);

private:
    static QString iconPath(const QString& name);
};

#endif // ICONHELPER_H
