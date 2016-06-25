#include "MainWindow.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    Data data;
    Analysis anal(data);

    {
        Function func;
        func.entry = 0x401000;
        func.ready = true;
        func.update_id = 0;

        Block block;
        block.entry = func.entry;
        block.header_text = Text("test1234", Qt::red, block.entry);
        Instr instr;
        instr.addr = block.entry;
        instr.opcode.push_back(0x90);
        instr.text = Text("test 2", Qt::blue, instr.addr);

        func.blocks.push_back(block);

        anal.data.entry = 0x401000;
        anal.functions.insert({func.entry, func});
    }

    mDisassemblerView = new DisassemblerView(anal, this);
    ui->centralWidget->layout()->addWidget(mDisassemblerView);
}

MainWindow::~MainWindow()
{
    delete ui;
}
