#include "MainWindow.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    Data data;
    data.entry = 0x401000;
    Analysis anal(data);
    {
        Function func;
        func.entry = 0x401000;
        func.ready = true;
        func.update_id = 0;
        {
            Block block;
            block.entry = func.entry;
            block.header_text = Text("block.header_text", Qt::red, block.entry);
            {
                Instr instr;
                instr.addr = block.entry;
                instr.opcode.push_back(0x90);
                instr.text = Text("block.instr", Qt::blue, instr.addr);
                block.instrs.push_back(instr);
            }
            func.blocks.push_back(block);
        }
        anal.functions.insert({func.entry, func});
    }

    mDisassemblerView = new DisassemblerView(anal, this);
    ui->centralWidget->layout()->addWidget(mDisassemblerView);
}

MainWindow::~MainWindow()
{
    delete ui;
}
