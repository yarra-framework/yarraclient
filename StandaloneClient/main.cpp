#include "sac_mainwindow.h"
#include <QtCore>
#include <QtGui>
#include <QApplication>
#include <QStyleFactory>
#include "sac_batchdialog.h"


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Parse command line options
    QCommandLineParser parser;
    parser.setApplicationDescription("SAC client");
    parser.addHelpOption();
    QCommandLineOption batchSubmitOption("b","batch file name","batch","");
    parser.addOption(batchSubmitOption);
    parser.process(a);
    bool batchSubmit = parser.isSet(batchSubmitOption);

    // Set color scheme
    qApp->setStyle(QStyleFactory::create("Fusion"));
    QPalette palette=QPalette(QColor(240,240,240),QColor(240,240,240));

    palette.setColor(QPalette::Window, QColor(53, 53, 53));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::WindowText,
                     QColor(127, 127, 127));
    palette.setColor(QPalette::Base, QColor(42, 42, 42));
    palette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
    palette.setColor(QPalette::ToolTipBase, Qt::white);
    palette.setColor(QPalette::ToolTipText, QColor(53, 53, 53));
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    palette.setColor(QPalette::Dark, QColor(35, 35, 35));
    palette.setColor(QPalette::Shadow, QColor(20, 20, 20));
    palette.setColor(QPalette::Button, QColor(53, 53, 53));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                     QColor(127, 127, 127));
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(42, 130, 218));

    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText,
                     QColor(127, 127, 127));

    palette.setColor(QPalette::Highlight, QColor(67,176,42));
    qApp->setPalette(palette);

    QFont defaultFont = QApplication::font();
    defaultFont.setPointSize(defaultFont.pointSize()+1);
    a.setFont(defaultFont);

    bool restartApp=true;
    int returnValue=0;

    while (restartApp)
    {
        restartApp=false;

        sacMainWindow* w=new sacMainWindow(NULL,batchSubmit);
        if (!batchSubmit) {
            w->show();
            returnValue=a.exec();
            restartApp=w->restartApp;
            delete w;
            w=0;
        } else {
            if (!w->didStart) {
                qInfo() << "Startup error";
                return 1;
            }
            QStringList files_l{};
            QStringList modes_l{};
            qInfo() << "Reading batch file...";
            QString file = parser.value(batchSubmitOption);
            QString notify;
            TaskPriority priority;
            if (! w->readBatchFile(file,files_l, modes_l, notify, priority))
            {
                return 1;
            }

            qInfo() << "Done reading batch file.";
            w->submitBatch(files_l,modes_l,notify,priority);
        }
    }

    return returnValue;
}
