#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "inferpage.h"

#include <QMenuBar>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , _infer_page(nullptr)
{
    ui->setupUi(this);

    _infer_page = new InferPage(this);
    setCentralWidget(_infer_page);

    setWindowTitle("心电异常检测桌面客户端");
    resize(980, 760);

    if (menuBar() != nullptr) {
        menuBar()->hide();
    }
    if (statusBar() != nullptr) {
        statusBar()->hide();
    }
}

MainWindow::~MainWindow() {
    delete ui;
}
