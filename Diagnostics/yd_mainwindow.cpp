#include <QScrollbar>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextDocument>

#include "yd_mainwindow.h"
#include "ui_yd_mainwindow.h"
#include "yd_global.h"

#include "yd_test.h"
#include "yd_test_systeminfo.h"
#include "yd_test_syngo.h"
#include "yd_test_yarra.h"
#include "yd_test_ort.h"
#include "yd_test_rds.h"
#include "yd_test_logserver.h"
#include "yd_test_logs.h"


ydMainWindow::ydMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ydMainWindow)
{
    ui->setupUi(this);

    Qt::WindowFlags flags = windowFlags();
    flags |= Qt::MSWindowsFixedSizeDialogHint;
    flags &= ~Qt::WindowContextHelpButtonHint;
    setWindowFlags(flags);

    setWindowTitle("Yarra - Diagnostics");
    setWindowIcon(YD_ICON);

    QString versionText = QString("<html><head/><body><p><span style=\" font-size:10pt; font-weight:600;\">Yarra Diagnostics</span><span style=\" font-size:10pt;\"><br/>Version %1</span></p></body></html>").arg(YD_VERSION);
    ui->versionLabel->setText(versionText);

    ui->runButton->setFocus();

    connect(&updateTimer, SIGNAL(timeout()), this, SLOT(timerCall()));
    ui->runButton->setText("Run Diagnostics");

    ui->issuesEdit->clear();
    ui->allResultsEdit->clear();
    ui->tabWidget->setCurrentIndex(0);
    ui->progressBar->setTextVisible(false);
    ui->progressBar->setVisible(false);

    composeTests();
}


ydMainWindow::~ydMainWindow()
{
    delete ui;
}


void ydMainWindow::composeTests()
{
    testRunner.testList.append(new ydTestSyngo);
    testRunner.testList.append(new ydTestYarra);
    testRunner.testList.append(new ydTestORT);
    testRunner.testList.append(new ydTestRDS);
    testRunner.testList.append(new ydTestLogServer);
    testRunner.testList.append(new ydTestSysteminfo);
    testRunner.testList.append(new ydTestLogs);
}


void ydMainWindow::on_runButton_clicked()
{
    if (testRunner.isActive)
    {
        if (!testRunner.isTerminating)
        {
            testRunner.cancelTests();
            ui->runButton->setText("Terminating...");
            ui->runButton->setEnabled(false);
        }
    }
    else
    {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QApplication::processEvents();

        ui->tabWidget->setCurrentIndex(1);
        ui->progressBar->setValue(0);
        ui->progressBar->setEnabled(true);
        ui->progressBar->setTextVisible(true);
        ui->progressBar->setVisible(true);
        ui->runButton->setText("Cancel");
        ui->issuesEdit->clear();
        ui->allResultsEdit->clear();
        testRunner.runTests();
        updateTimer.start(50);
    }
}


void ydMainWindow::timerCall()
{
    // Update UI
    ui->progressBar->setValue(testRunner.getPercentage());
    ui->allResultsEdit->setHtml(testRunner.results);
    ui->allResultsEdit->verticalScrollBar()->setValue(ui->allResultsEdit->verticalScrollBar()->maximum());

    if (!testRunner.isActive)
    {
        updateTimer.stop();
        ui->progressBar->setValue(0);
        ui->progressBar->setEnabled(false);
        ui->progressBar->setTextVisible(false);
        ui->progressBar->setVisible(false);
        ui->runButton->setText("Run Diagnostics");
        ui->runButton->setEnabled(true);
        ui->allResultsEdit->verticalScrollBar()->setValue(0);
        ui->tabWidget->setCurrentIndex(0);
        ui->issuesEdit->setText(testRunner.issues);

        ui->exportButton->setEnabled(true);
        QApplication::restoreOverrideCursor();

        ui->allResultsEdit->setHtml(testRunner.results);
        ui->allResultsEdit->verticalScrollBar()->setValue(0);
    }
}

void ydMainWindow::on_exportButton_clicked()
{
    QString suggestedFilename = "report_" + QDateTime::currentDateTime().toString("MMddyy-HHmmss")+".html";
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Report"), suggestedFilename, "HTML Report(*.html)");

    if (fileName.isEmpty())
    {
        return;
    }

    QFile reportFile(fileName);
    if (!reportFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, "Error", "Unable to create and save the report. Please use differnet filename and/or folder.");
        return;
    }

    // Quick and dirty generation of HTML file (but more flexible than using Qt internals)
    QString fileContent="";
    fileContent += "<!doctype html>\n" \
                   "<html lang=\"en\">\n" \
                   "<head>"\
                   "<meta charset=\"utf-8\">" \
                   "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n" \
                   "<title>Yarra Diagnostics Report</title>\n" \
                   "</head>\n" \
                   "<style> body { background-color: #2A2A2A; color: #FFF; font-size: 14px; font-family: Helvetica,Arial,sans-serif; padding: 10px; } "\
                           "hr { border: 0; border-top: 2px solid #424242; } " \
                           ".output { margin-left: 40px; margin-right: 40px; margin-bottom: 40px; padding-left: 20px; padding-right: 20px; padding-top: 10px; padding-bottom: 10px; background-color: #232323; }" \
                           "h3 { color: #CCC; padding-left: 8px; border-left: 6px solid #7F7F7F; } h2 { color: #CCC; } " \
                           "a, a:visited, a:focus { color: #FFF; text-decoration: none; } a:hover { color: #E0A526; text-decoration: underline; } " \
                   "</style> \n" \
                   "<body>\n";

    fileContent += "<h2 style=\" margin-bottom: 30px; margin-top: 0px; \">Yarra Diagnostics Report</h2>\n";
    fileContent += "<h3>Detected Issues</h3>\n";
    fileContent += "<div class=\"output\">"+testRunner.issues+"</div>";
    fileContent += "<h3>All Results</h3>\n";
    fileContent += "<div class=\"output\">"+testRunner.results+"</div>";
    fileContent += "<p style=\" margin-top: 40px; font-weight: 200; color: #7F7F7F; text-align:center; \">Generated with Yarra Diagnostics, Version "+QString(YD_VERSION)+". For updates and support visit: <a style=\"color: #CCC\" href=\"https://yarra-framework.org\">https://yarra-framework.org</a></p>\n";
    fileContent += "</body>\n</html>\n";

    QTextStream outStream(&reportFile);
    outStream << fileContent;
    reportFile.close();

    QMessageBox::about(this, "Report Created", "The findings have been written into the report file.");
}
