#ifndef YCA_MAINWINDOW_H
#define YCA_MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include <QMenu>


namespace Ui {
class ycaMainWindow;
}

class ycaMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ycaMainWindow(QWidget *parent = 0);
    ~ycaMainWindow();

protected:
    void closeEvent(QCloseEvent* event);

public slots:
    void callShutDown();

private slots:
    void iconActivated(QSystemTrayIcon::ActivationReason reason);

    void on_closeButton_clicked();
    void on_closeContextButton_clicked();

private:
    Ui::ycaMainWindow* ui;

    QSystemTrayIcon* trayIcon;
    QMenu* trayIconMenu;
    QAction* trayItemShutdown;

};

#endif // YCA_MAINWINDOW_H
