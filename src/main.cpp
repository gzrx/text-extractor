#include <QCoreApplication>
#include <QDebug>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    qInfo() << "textract" << QCoreApplication::applicationVersion();
    return 0;
}
