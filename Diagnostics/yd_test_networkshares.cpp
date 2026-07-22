#include "yd_test_networkshares.h"
#include "yd_global.h"

#include "../Client/rds_exechelper.h"


ydTestNetworkShares::ydTestNetworkShares() : ydTest()
{
}


QString ydTestNetworkShares::getName()
{
    return "Network Shares";
}


QString ydTestNetworkShares::getDescription()
{
    return "list mapped Windows network shares and check their connection status";
}


bool ydTestNetworkShares::run(QString& issues, QString& results)
{
    YD_RESULT_STARTSECTION
    YD_ADDRESULT("<u>Mapped network shares:</u>")

    rdsExecHelper execHelper;
    QString command = "net use";
    execHelper.setCommand(command);
    execHelper.run();

    int shareCount = 0;

    for (int i=0; i<execHelper.output.length(); i++)
    {
        QString line = execHelper.output.at(i).trimmed();
        QStringList tokens = line.split(" ", QString::SkipEmptyParts);

        // A mapped share is listed as "<status> <drive letter> <UNC path> ...", e.g.
        // "OK           Z:        \\server\share            Microsoft Windows Network"
        // Skip any other line (header, separator, summary, or unmapped print queue entries).
        if (tokens.length() < 3)
        {
            continue;
        }
        if ((tokens.at(1).length()!=2) || (tokens.at(1).at(1)!=':'))
        {
            continue;
        }
        if (!tokens.at(2).startsWith("\\\\"))
        {
            continue;
        }

        QString status = tokens.at(0);
        QString drive  = tokens.at(1);
        QString remote = tokens.at(2);
        shareCount++;

        YD_ADDRESULT_LINE("&nbsp;");
        if (status.compare("OK", Qt::CaseInsensitive)==0)
        {
            YD_ADDRESULT_COLORLINE(drive + " &rarr; " + remote + " (status: " + status + ")", YD_SUCCESS);
        }
        else
        {
            YD_ADDRESULT_COLORLINE(drive + " &rarr; " + remote + " (status: " + status + ")", YD_WARNING);
            YD_ADDISSUE("Network share " + drive + " (" + remote + ") has status '" + status + "'", YD_WARNING);
        }
    }

    if (shareCount==0)
    {
        YD_ADDRESULT_LINE("No mapped network shares found");
    }

    YD_RESULT_ENDSECTION

    return true;
}
