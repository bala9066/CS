#include "iconhelper.h"
#include <QApplication>
#include <QPainter>
#include <QPixmap>
#include <QDebug>
#include <QFile>
#include <QSize>
#include <QBrush>
#include <QRadialGradient>

IconHelper::IconHelper(QObject* parent)
    : QObject(parent)
{
}

QString IconHelper::iconPath(const QString& name) {
    // Try .ico from resources first (higher priority)
    QString icoPath = QString(":/icons/%1.ico").arg(name);
    if (QFile::exists(icoPath)) return icoPath;

    // Try .svg from resources
    QString svgPath = QString(":/icons/%1.svg").arg(name);
    if (QFile::exists(svgPath)) return svgPath;

    // Fallback to local .ico
    icoPath = QString("resources/icons/%1.ico").arg(name);
    if (QFile::exists(icoPath)) return icoPath;

    // Fallback to local .svg
    svgPath = QString("resources/icons/%1.svg").arg(name);
    if (QFile::exists(svgPath)) return svgPath;

    return QString();
}

QIcon IconHelper::loadIcon(const QString& name) {
    QString path = iconPath(name);

    if (!path.isEmpty()) {
        QIcon icon(path);
        if (!icon.isNull()) {
            return icon;
        }
    }

    // Return fallback icon
    return QIcon(getFallbackIcon("primary", 16));
}

QPushButton* IconHelper::createIconButton(const QString& iconName, const QString& text) {
    QPushButton* button = new QPushButton(text);

    QIcon icon = loadIcon(iconName);
    if (!icon.isNull()) {
        button->setIcon(icon);
        button->setIconSize(QSize(16, 16));
    }

    return button;
}

QLabel* IconHelper::createStatusIcon(const QString& status) {
    QLabel* label = new QLabel();
    label->setProperty("status", status);

    // Try to load status icon
    QString iconName = status; // success, error, warning, info
    QIcon icon = loadIcon(iconName);

    if (!icon.isNull()) {
        label->setPixmap(icon.pixmap(16, 16));
    } else {
        // Use text fallback
        QString emoji;
        if (status == "success") emoji = "✓";
        else if (status == "error") emoji = "✗";
        else if (status == "warning") emoji = "⚠";
        else if (status == "info") emoji = "ℹ";
        else emoji = "?";

        label->setText(emoji);
    }

    return label;
}

QLabel* IconHelper::createIconLabel(const QString& iconName, int size) {
    QLabel* label = new QLabel();

    QIcon icon = loadIcon(iconName);
    if (!icon.isNull()) {
        label->setPixmap(icon.pixmap(size, size));
    }

    return label;
}

QPixmap IconHelper::getFallbackIcon(const QString& type, int size) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor color;
    if (type == "primary") color = QColor(99, 102, 241);    // #6366F1
    else if (type == "success") color = QColor(34, 197, 94); // #22C55E
    else if (type == "danger") color = QColor(239, 68, 68);  // #EF4444
    else if (type == "warning") color = QColor(245, 158, 11); // #F59E0B
    else color = QColor(148, 163, 184);                      // #94A3B8

    // Draw rounded rectangle with gradient
    QRadialGradient gradient(size/2, size/2, size/2);
    gradient.setColorAt(0, color);
    gradient.setColorAt(1, color.darker(150));

    painter.setBrush(QBrush(gradient));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(0, 0, size, size, size/4, size/4);

    return pixmap;
}
