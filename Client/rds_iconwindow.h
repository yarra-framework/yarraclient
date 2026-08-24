#ifndef RDS_ICONWINDOW_H
#define RDS_ICONWINDOW_H

#include <QDialog>
#include <QProxyStyle>

namespace Ui
{
    class rdsIconWindow;
}


class rdsIconProxyStyle: public QProxyStyle
{
    Q_OBJECT
public:
    rdsIconProxyStyle(QStyle* style=0) : QProxyStyle(style)
    {
    }

    rdsIconProxyStyle(const QString& key) : QProxyStyle(key)
    {
    }

    virtual int pixelMetric(QStyle::PixelMetric metric, const QStyleOption* option = 0, const QWidget* widget = 0 ) const
    {
        switch ( metric )
        {
        case QStyle::PM_SmallIconSize:
            return 32;
          default:
            return QProxyStyle::pixelMetric( metric, option, widget );
        }
    }
};


class rdsIconWindow : public QDialog
{
    Q_OBJECT

public:
    explicit rdsIconWindow(QWidget *parent = 0);
    ~rdsIconWindow();

    void setAnim(bool value);
    void clearError();
    void setError();

    void showStartupCommandsOption();
    void showORTOption();
    void showCloudWindowOption();
    void showFileExplorerOption();
    void showDiagnosticsOption();

private:
    Ui::rdsIconWindow* ui;

    void mouseDoubleClickEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

    bool    animRunning;
    bool    error;
    QMovie* anim;
    bool    showStartupCommandsEntry;
    bool    showCloudWindowEntry;
    bool    showORTEntry;
    bool    showFileExplorerEntry;
    bool    showDiagnosticsEntry;

private slots:
    void showStatusWindow();
    void showCloudAgent();
    void showFileExplorer();
    void startORTClient();
    void runStartupCommands();
    void triggerTransferNow();
    void startDiagnostics();

};


#endif // RDS_ICONWINDOW_H

