#include "DisassemblerView.h"
#include <vector>
#include <QPainter>
#include <QScrollBar>

DisassemblerView::DisassemblerView(QWidget* parent, const Data & data, const View & view)
    : QAbstractScrollArea(parent),
      view(view),
      data(data),
      analysis(analysis)
{
    this->status = "";

    //Create analysis and start it in another thread
    //Dummy

    //Start disassembly view at the entry point of the binary
    this->function = this->data.entry;
    //this->update_id = None;
    this->ready = false;
    //this->desired_pos = None;
    this->highlight_token = nullptr;
    this->cur_instr = 0;
    //this->scroll_mode = false;
    this->blocks.clear();
    //this->show_il = false;
    //this->simulation = None;

    //Create timer to automatically refresh view when it needs to be updated
    this->updateTimer = new QTimer();
    this->updateTimer->setInterval(100);
    this->updateTimer->setSingleShot(false);
    connect(this->updateTimer, SIGNAL(timeout()), this, SLOT(updateTimerEvent()));
    this->updateTimer->start();

    //Initialize scroll bars
    this->width = 0;
    this->height = 0;
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    this->horizontalScrollBar()->setSingleStep(this->charWidth);
    this->verticalScrollBar()->setSingleStep(this->charHeight);
    this->areaSize = this->viewport()->size();
}

void DisassemblerView::initFont()
{
    setFont(QFont("Courier New", 8));
    QFontMetricsF metrics(this->font());
    this->baseline = int(metrics.ascent());
    this->charWidth = metrics.width('X');
    this->charHeight = metrics.height();
    this->charOffset = 0;
}

void DisassemblerView::adjustSize(int width, int height)
{
    //Recompute size information
    this->renderWidth = width;
    this->renderHeight = height;
    this->renderXOfs = 0;
    this->renderYOfs = 0;
    if(this->renderWidth < width) //TODO: dead
    {
        this->renderXOfs = (width - this->renderWidth) / 2;
        this->renderWidth = width;
    }
    if(this->renderHeight < height) //TODO: dead
    {
        this->renderYOfs = (height - this->renderHeight) / 2;
        this->renderHeight = height;
    }
    //Update scroll bar information (TODO: dead)
    this->horizontalScrollBar()->setPageStep(width);
    this->horizontalScrollBar()->setRange(0, this->renderWidth - width);
    this->verticalScrollBar()->setPageStep(height);
    this->verticalScrollBar()->setRange(0, this->renderHeight - height);
}

void DisassemblerView::resizeEvent(QResizeEvent *event)
{
    adjustSize(event->size().width(), event->size().height());
}

void DisassemblerView::updateTimerEvent()
{
    //TODO
}

void DisassemblerView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this->viewport());
    p.setFont(this->font());

    int xofs = this->horizontalScrollBar()->value();
    int yofs = this->verticalScrollBar()->value();

    if(!this->ready)
    {
        //Analysis for the current function is not yet complete, paint loading screen
        QLinearGradient gradient = QLinearGradient(QPointF(0, 0),
                                                   QPointF(this->viewport()->size().height(), this->viewport()->size().height()));
        gradient.setColorAt(0, QColor(232, 232, 232));
        gradient.setColorAt(1, QColor(192, 192, 192));
        p.setPen(QColor(0, 0, 0, 0));
        p.setBrush(QBrush(gradient));
        p.drawRect(0, 0, this->viewport()->size().width(), this->viewport()->size().height());

        QString text = this->function ? "Loading..." : "No function selected";
        p.setPen(Qt::black);
        p.drawText((this->viewport()->size().width() / 2) - ((text.length() * this->charWidth) / 2),
                   (this->viewport()->size().height() / 2) + this->charOffset + this->baseline - (this->charHeight / 2),
                   text);
        return;
    }

    //Render background
    QLinearGradient gradient = QLinearGradient(QPointF(-xofs, -yofs), QPointF(this->renderWidth - xofs, this->renderHeight - yofs));
    gradient.setColorAt(0, QColor(232, 232, 232));
    gradient.setColorAt(1, QColor(192, 192, 192));
    p.setPen(QColor(0, 0, 0, 0));
    p.setBrush(QBrush(gradient));
    p.drawRect(0, 0, this->viewport()->size().width(), this->viewport()->size().height());

    p.translate(this->renderXOfs - xofs, this->renderYOfs - yofs);

    //Render each node
    for(auto & blockIt : this->blocks)
    {
        DisassemberBlock block = blockIt.second;
        //Render shadow
        p.setPen(QColor(0, 0, 0, 0));
        p.setBrush(QColor(0, 0, 0, 128));
        p.drawRect(block.x + this->charWidth + 4, block.y + this->charWidth + 4,
                   block.width - (4 + 2 * this->charWidth), block.height - (4 + 2 * this->charWidth));

        //Render node background
        QLinearGradient gradient = QLinearGradient(QPointF(0, block.y + this->charWidth),
                                                   QPointF(0, block.y + block.height - this->charWidth));
        gradient.setColorAt(0, QColor(255, 255, 252));
        gradient.setColorAt(1, QColor(255, 255, 232));
        p.setPen(Qt::black);
        p.setBrush(QBrush(gradient));
        p.drawRect(block.x + this->charWidth, block.y + this->charWidth,
                   block.width - (4 + 2 * this->charWidth), block.height - (4 + 2 * this->charWidth));

        //Print current instruction background
        if (this->cur_instr != 0)
        {
            int y = block.y + (2 * this->charWidth) + (int(block.block.header_text.lines.size()) * this->charHeight);
            for(Instr & instr : block.block.instrs)
            {
                if(instr.addr == this->cur_instr)
                {
                    p.setPen(QColor(0, 0, 0, 0));
                    p.setBrush(QColor(255, 255, 128, 128));
                    p.drawRect(block.x + this->charWidth + 3, y, block.width - (10 + 2 * this->charWidth),
                               int(instr.text.lines.size()) * this->charHeight);
                }
                y += int(instr.text.lines.size()) * this->charHeight;
            }
        }

        //Render highlighted tokens
        if(this->highlight_token)
        {
            int x = block.x + (2 * this->charWidth);
            int y = block.y + (2 * this->charWidth);
            for(auto & line : block.block.header_text.tokens)
            {
                for(Token & token : line)
                {
                    if(this->highlight_token->equalsToken(token))
                    {
                        p.setPen(QColor(0, 0, 0, 0));
                        p.setBrush(QColor(192, 0, 0, 64));
                        p.drawRect(x + token.start * this->charWidth, y,
                                   token.length * this->charWidth, this->charHeight);
                    }
                }
                y += this->charHeight;
            }
            for(auto & instr : block.block.instrs)
            {
                for(auto & line : instr.text.tokens)
                {
                    for(Token & token : line)
                    {
                        if(this->highlight_token->equalsToken(token))
                        {
                            p.setPen(QColor(0, 0, 0, 0));
                            p.setBrush(QColor(192, 0, 0, 64));
                            p.drawRect(x + token.start * this->charWidth, y,
                                       token.length * this->charWidth, this->charHeight);
                        }
                    }
                    y += this->charHeight;
                }
            }
        }

        //Render node text
        auto x = block.x + (2 * this->charWidth);
        auto y = block.y + (2 * this->charWidth);
        for(auto & line : block.block.header_text.lines)
        {
            auto partx = x;
            for(Line & part : line)
            {
                p.setPen(part.color);
                p.drawText(partx, y + this->charOffset + this->baseline, part.text);
                partx += part.text.length() * this->charWidth;
            }
            y += this->charHeight;
        }
        for(auto & instr : block.block.instrs)
        {
            for(auto & line : instr.text.lines)
            {
                auto partx = x;
                for(Line & part : line)
                {
                    p.setPen(part.color);
                    p.drawText(partx, y + this->charOffset + this->baseline, part.text);
                    partx += part.text.length() * this->charWidth;
                }
                y += this->charHeight;
            }
        }

        // Render edges
        for(DisassemblerEdge edge : block.edges)
        {
            p.setPen(edge.color);
            p.setBrush(edge.color);
            p.drawPolyline(edge.polyline);
            p.drawConvexPolygon(edge.arrow);
        }
    }
}
