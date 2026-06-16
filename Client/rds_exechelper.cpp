#include <QTimer>

#include "rds_exechelper.h"
#include "rds_global.h"


rdsExecHelper::rdsExecHelper()
{
    process.setReadChannel(QProcess::StandardOutput);
    process.setProcessChannelMode(QProcess::MergedChannels);

    timeoutMs=RDS_PROC_TIMEOUT;
    output.clear();

    cmdLine="";

    monitorNetUseOutput  =false;
    detectedNetUseError  =false;
    detectedNetUseSuccess=false;
    detectedNetUseError="";

    exitCode=1;
}


bool rdsExecHelper::run()
{
    return run(cmdLine);
}


bool rdsExecHelper::run(QString cmdLine, QString nativeArguments)
{
    // Clear the output buffer
    output.clear();
    exitCode = 1;

    QProcess *myProcess=new QProcess(0);
    myProcess->setReadChannel(QProcess::StandardOutput);

    bool success=false;

    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.setInterval(timeoutMs);
    QEventLoop q;
    QObject::connect(myProcess, SIGNAL(finished(int, QProcess::ExitStatus)), &q, SLOT(quit()));
    QObject::connect(&timeoutTimer, SIGNAL(timeout()), &q, SLOT(quit()));

    // Time measurement to diagnose RaidTool calling problems
    QElapsedTimer ti;
    ti.start();
    timeoutTimer.start();
    if (!nativeArguments.isEmpty()){
        // https://www.qthub.com/static/doc/qt5/qtcore/qprocess.html#setNativeArguments
        // > Note: This function is available only on the Windows platform.
        // 2025-06-15 debian Qt 5.15.8
        // error: ‘class QProcess’ has no member named ‘setNativeArguments’; did you mean ‘setArguments’?
        #ifdef Q_OS_WIN
        myProcess->setNativeArguments(nativeArguments);
        #else
        // splitting on space is going to be a problem for e.g. "quoted arguments with space"!
        myProcess->setArguments(nativeArguments.split(' '));
        #endif
    }
//    else {
//        myProcess->start(cmdLine, arguments);
//    }
    myProcess->start(cmdLine);

    if (myProcess->state()==QProcess::NotRunning)
    {
        delete myProcess;
        myProcess=0;
        RTI->log("ERROR: Process did not start - '" + cmdLine + "'");
        return false;
    }
    else
    {
        q.exec();
    }

    // Check for problems with the event loop: Sometimes it seems to return to quickly!
    // In this case, start a second while loop to check when the process is really finished.
    if ((timeoutTimer.isActive()) && (myProcess->state()==QProcess::Running))
    {
        timeoutTimer.stop();
        RTI->log("Warning: QEventLoop returned too early. Starting secondary loop.");
        while ((myProcess->state()==QProcess::Running) && (ti.elapsed()<timeoutMs))
        {
            RTI->processEvents();
            Sleep(RDS_SLEEP_INTERVAL);
        }

        // If the process did not finish within the timeout duration
        if (myProcess->state()==QProcess::Running)
        {
            RTI->log("Warning: Process is still active. Killing process.");
            myProcess->kill();
            success=false;
        }
        else
        {
            success=true;
        }
    }
    else
    {
        // Normal timeout-handling if QEventLoop works normally
        if (timeoutTimer.isActive())
        {
            success=true;
            timeoutTimer.stop();
        }
        else
        {
            RTI->log("Warning: Process event loop timed out.");
            RTI->log("Warning: Duration since start "+QString::number(ti.elapsed())+" ms");
            success=false;
            if (myProcess->state()==QProcess::Running)
            {
                RTI->log("Warning: Process is still active. Killing process.");
                myProcess->kill();
            }
        }
    }

    exitCode = myProcess->exitCode();

    if (!success)
    {
        // The process timeed out. Probably some error occured.
        RTI->log("ERROR: Timeout during cmd execution!");
    }
    else
    {
        // Read the output lines from the process and store in string list
        char buf[1024];
        qint64 lineLength = -1;

        do
        {
            lineLength=myProcess->readLine(buf, sizeof(buf));
            if (lineLength != -1)
            {
                output << QString(buf);
            }
        } while (lineLength!=-1);
    }

    delete myProcess;
    myProcess=0;

    return success;
}


bool rdsExecHelper::callNetUseTimout(int timeoutMs)
{
    bool success=false;
    exitCode=1;
    detectedNetUseErrorMessage="";

    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.setInterval(timeoutMs);

    QEventLoop q;
    connect(&process, SIGNAL(finished(int , QProcess::ExitStatus)), &q, SLOT(quit()));
    connect(&process, SIGNAL(error(QProcess::ProcessError)), &q, SLOT(quit()));
    connect(&timeoutTimer, SIGNAL(timeout()), &q, SLOT(quit()));

    if (monitorNetUseOutput)
    {
        connect(&process, SIGNAL(readyReadStandardOutput()), this, SLOT(readNetUseOutput()));
    }

    // Time measurement to diagnose RaidTool calling problems
    QElapsedTimer ti;
    ti.start();
    timeoutTimer.start();

    // Start the process. Note: The commandline and arguments need to be defined before.
    process.start(cmdLine);
    if (process.state()==QProcess::Running)
    {
        q.exec();
    }

    // Check for problems with the event loop: Sometimes it seems to return to quickly!
    // In this case, start a second while loop to check when the process is really finished.
    if ((timeoutTimer.isActive()) && (process.state()==QProcess::Running))
    {
        timeoutTimer.stop();
        RTI->log("Warning: QEventLoop returned too early. Starting secondary loop.");
        while ((process.state()==QProcess::Running) && (ti.elapsed()<timeoutMs))
        {
            RTI->processEvents();
            Sleep(RDS_SLEEP_INTERVAL);
        }

        // If the process did not finish within the timeout duration
        if (process.state()==QProcess::Running)
        {
            RTI->log("Warning: Process is still active. Killing process.");
            process.kill();
            success=false;
        }
        else
        {
            success=true;
        }
    }
    else
    {
        // Normal timeout-handling if QEventLoop works normally
        if (timeoutTimer.isActive())
        {
            success=true;
            timeoutTimer.stop();
        }
        else
        {
            RTI->log("Warning: Process event loop timed out.");
            RTI->log("Warning: Duration since start "+QString::number(ti.elapsed())+" ms");
            success=false;
            if (process.state()==QProcess::Running)
            {
                RTI->log("Warning: Process is still active. Killing process.");
                process.kill();
            }
        }
    }

    exitCode=process.exitCode();

    if (monitorNetUseOutput)
    {
        readNetUseOutput();

        if (detectedNetUseSuccess)
        {
            success=true;
        }
        if (detectedNetUseError)
        {
            success=false;
        }
    }

    return success;
}


void rdsExecHelper::readNetUseOutput()
{
    detectedNetUseError  =false;
    detectedNetUseSuccess=false;

    while (process.canReadLine())
    {
        // Read the current line, but restrict the maximum length to 512 chars to
        // avoid infinite output (if a module starts outputting binary data)
        QString currentLine=process.readLine(512);

        // Mapping or deletion sucessfull
        if (currentLine.contains("successfully."))
        {
            detectedNetUseSuccess=true;
        }

        // Deletion of non-existing mapping --> Treated as success
        if (currentLine.contains("The network connection could not be found."))
        {
            detectedNetUseSuccess=true;
        }

        // Error caused, either by mapping of deletion
        if (currentLine.contains("System error"))
        {
            detectedNetUseError=true;
            RTI->log("Error detected: "+currentLine);
            detectedNetUseErrorMessage=currentLine;
        }
    }
}


void rdsExecHelper::safeSleep(int ms)
{
    QElapsedTimer ti;
    ti.start();
    while (ti.elapsed()<ms)
    {
        RTI->processEvents();
        Sleep(RDS_SLEEP_INTERVAL);
    }
}

