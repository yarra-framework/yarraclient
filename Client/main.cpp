#include <QApplication>
#include <QWidget>
#include <QMessageBox>

#include "rds_global.h"
#include "rds_configurationwindow.h"
#include "rds_operationwindow.h"
#include "main.h"


rdsApplication::rdsApplication(int &argc, char **argv, bool GUIenabled)
    : QtSingleApplication(argc, argv, GUIenabled)
{
    connect(this, SIGNAL(messageReceived(const QString&)), this, SLOT(respond(const QString&)));
}


void rdsApplication::respond(const QString &message)
{
    if (message=="")
    {
        //RTI->log("Received show message!");
        //activateWindow();
        ((rdsOperationWindow*) activationWindow())->showNormal();
    }

    if (message=="update")
    {
        if (activationWindow()!=0)
        {
            ((rdsOperationWindow*) activationWindow())->callManualUpdate();
        }
    }
}


int main(int argc, char *argv[])
{
    // Qt5 does not scale widget layouts to the display's DPI setting unless
    // explicitly enabled (Qt6 does this by default). Must be set before the
    // QApplication instance is constructed.
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    // NOTE: A slight modification has been introduced to QtSingleApplication
    //       in the .activateWindow() method. It was necessary to add a call
    //       to .show() because otherwise the operation window remains hidden.
    rdsApplication a(argc, argv);
    Q_INIT_RESOURCE(rds);

    // If the application is already running, send a message instead of launching
    // another instance
    if (a.isRunning())
    {
        QString msg="";

        if (argc>1)
        {
            if (!qstrcmp(argv[1], "-update"))
            {
                // If the program is called with the argument "-update", send an update
                // message so that the running instance performs the update now
                msg="update";
            }
            else
            {
                // For any other argument, don't do anything and terminate silently.
                return 0;
            }
        }

        // If not called with an argument, show the operations window
        // by sending an empty message

        a.sendMessage(msg);

        // Shutdown this instance
        return 0;
    }

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

    palette.setColor(QPalette::Highlight, QColor(88,15,139));
    qApp->setPalette(palette);

    QFont defaultFont = QApplication::font();
    defaultFont.setPointSize(defaultFont.pointSize()+1);
    a.setFont(defaultFont);

    // Prevent termination of the application if no window is visible
    qApp->setQuitOnLastWindowClosed(false);

    RTI->prepare();

    if (RTI->isInvalidEnvironment())
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Invalid Runtime Environment");
        msgBox.setText("This program can only be used on the host computer \n of a Siemens MAGNETOM system.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setWindowIcon(RDS_ICON);
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();

        return 0;
    }

    if (!RTI->prepareEnvironment())
    {
        return 0;
    }


    rdsConfigurationWindow* cw=0;
    rdsOperationWindow* ow=0;

    bool firstRun=true;

    while (RTI->getMode()!=rdsRuntimeInformation::RDS_QUIT)
    {
        switch (RTI->getMode())
        {
        case rdsRuntimeInformation::RDS_OPERATION:
            ow=new rdsOperationWindow(0,firstRun);
            a.setActivationWindow(ow, false);
            a.exec();
            a.setActivationWindow(0, false);
            RDS_FREE(ow);

            firstRun=false;
            break;

        case rdsRuntimeInformation::RDS_CONFIGURATION:
            cw=new rdsConfigurationWindow();
            cw->show();
            a.exec();
            RDS_FREE(cw);
            RTI->processEvents();
            break;

        case rdsRuntimeInformation::RDS_QUIT:
        default:
            break;
        }
    }
    return RTI->getReturnValue();
}

