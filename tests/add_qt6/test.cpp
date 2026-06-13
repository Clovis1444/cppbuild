#include <QDebug>

int main() {

#ifdef QT_VERSION_STR
    // qDebug() << "Qt version:" << QT_VERSION_STR;
    return 0;
#endif

    return 1;
}
