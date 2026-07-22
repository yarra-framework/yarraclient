#ifndef YDTESTNETWORKSHARES_H
#define YDTESTNETWORKSHARES_H

#include "yd_test.h"

class ydTestNetworkShares : public ydTest
{
public:
    ydTestNetworkShares();

    QString getName();
    QString getDescription();

    bool run(QString& issues, QString& results);
};

#endif // YDTESTNETWORKSHARES_H
