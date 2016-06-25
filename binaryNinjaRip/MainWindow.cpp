#include "MainWindow.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    Data data;
    Analysis anal(data);
    mDisassemblerView = new DisassemblerView(anal, this);
    ui->centralWidget->layout()->addWidget(mDisassemblerView);
}

MainWindow::~MainWindow()
{
    delete ui;
}
