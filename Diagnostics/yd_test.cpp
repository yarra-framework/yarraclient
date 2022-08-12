#include "yd_test.h"
#include "yd_global.h"


ydTest::ydTest()
{
}


QString ydTest::getName()
{
    return "Base";
}


QString ydTest::getDescription()
{
    return "";
}


QString ydTest::getResults()
{
    return "OK";
}


QString ydTest::getIssues()
{
    return "Nothing";
}


bool ydTest::run()
{
    return true;
}



ysTestThread::ysTestThread(ydTestRunner* myParent)
{
    runner=myParent;
    currentIndex=0;
}


void ysTestThread::run()
{
    bool cancelled=false;

    runner->results="<p>Running diagnostics at "+QDateTime::currentDateTime().toString()+"</p>";
    runner->issues="";

    for (int i=0; i<runner->testList.count(); i++)
    {
        if (isInterruptionRequested())
        {
            cancelled=true;
            break;
        }

        runner->results+="<hr><p>Now running test '" + runner->testList.at(i)->getName() + "'...<br />Description: " + runner->testList.at(i)->getDescription() + "</p>";
        runner->results+="<p>" + runner->testList.at(i)->getResults() + "</p>";
        runner->issues="";

        currentIndex=i;
        runner->testList.at(i)->run();

        if (isInterruptionRequested())
        {
            cancelled=true;
            break;
        }
    }
    if (cancelled)
    {
        runner->results+="<hr><p><strong>Cancelled.</strong></p>";
    }
    else
    {
        runner->results+="<hr><p><strong>Done.</strong></p>";
    }

    if (runner->issues.isEmpty())
    {
        runner->issues="<p>No issues found</p>";
    }

    runner->isTerminating=false;
    runner->isActive=false;
    quit();
}


ydTestRunner::ydTestRunner() : testThread(this)
{
    results="";
    issues="";
    isActive=false;
}


bool ydTestRunner::runTests()
{
    if (isActive)
    {
        return false;
    }
    isTerminating=false;
    isActive=true;    
    testThread.start();
    return true;
}


bool ydTestRunner::cancelTests()
{
    if (!isActive)
    {
        return false;
    }
    testThread.requestInterruption();
    isTerminating=true;
    return true;
}


int ydTestRunner::getPercentage()
{
    if (testList.isEmpty())
    {
        return 0;
    }

    return int(100.*(testThread.currentIndex)/double(testList.count()));
}

