#include "mainwindow.h"
#include <QApplication>
#include <QIcon>
#include <QCoreApplication>

int main(int argc, char *argv[]) {
    // Enable High-DPI scaling for better display on high-DPI screens
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setApplicationName("VDDAuditSystem");
    app.setOrganizationName("DevCorp");
    app.setWindowIcon(QIcon(":/icons/checksum.ico"));

    MainWindow w;
    w.show();
    QApplication::processEvents();
    w.showMaximized();

    return app.exec();
}
