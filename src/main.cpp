#include "MainWindow.h"
#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("ZC95 Lua Builder");
    QApplication::setOrganizationName("zc95");

    MainWindow w;
    w.show();
    return app.exec();
}
