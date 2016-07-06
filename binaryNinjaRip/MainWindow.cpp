#include "MainWindow.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    Data data;
    data.entry = 10;
    Analysis anal(data);
    {
        Function func;
        func.entry = 10;
        func.ready = true;
        func.update_id = 0;
        {
            Block block1;
            block1.entry = 10;
            block1.header_text = Text("block1.header_text", Qt::red, block1.entry);
            {
                Instr instr;
                instr.addr = block1.entry;
                instr.opcode.push_back(0x90);
                instr.text = Text("block1.instr", Qt::blue, instr.addr);
                block1.instrs.push_back(instr);
            }
            block1.true_path = 20;
            block1.exits.push_back(block1.true_path);
            block1.false_path = block1.entry;
            block1.exits.push_back(block1.false_path);

            func.blocks.push_back(block1);
        }
        {
            Block block2;
            block2.entry = 20;
            block2.header_text = Text("block2.header_text", Qt::red, block2.entry);
            {
                Instr instr;
                instr.addr = block2.entry;
                instr.opcode.push_back(0x90);
                instr.text = Text("block2.instr", Qt::blue, instr.addr);
                block2.instrs.push_back(instr);
            }
            func.blocks.push_back(block2);
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
