#include "DisassemblerView.h"
#include <vector>
#include <QPainter>
#include <QScrollBar>
#include <QClipboard>
#include <QApplication>

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
    this->update_id = 0;
    this->ready = false;
    this->desired_pos = nullptr;
    this->highlight_token = nullptr;
    this->cur_instr = 0;
    this->scroll_base_x = 0;
    this->scroll_base_y = 0;
    this->scroll_mode = false;
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
    auto areaSize = this->viewport()->size();
    this->adjustSize(areaSize.width(), areaSize.height());
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

duint DisassemblerView::get_cursor_pos()
{
    if(this->cur_instr == 0)
        return this->function;
    return this->cur_instr;
}

void DisassemblerView::set_cursor_pos(duint addr)
{
    Q_UNUSED(addr);
    //TODO
}

std::tuple<duint, duint> DisassemblerView::get_selection_range()
{
    return std::make_tuple<duint, duint>(get_cursor_pos(), get_cursor_pos());
}

void DisassemblerView::set_selection_range(std::tuple<duint, duint> range)
{
    this->set_cursor_pos(std::get<0>(range));
}

bool DisassemblerView::write(const Data & data)
{
    duint pos = this->get_cursor_pos();
    if(pos == 0)
        return false;
    return this->data.write(data);
}

void DisassemblerView::copy_address()
{
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->clear();
    QMimeData mime;
    mime.setText(QString().sprintf("0x%p", this->get_cursor_pos()));
    clipboard->setMimeData(&mime);
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
        DisassemblerBlock block = blockIt.second;
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

bool DisassemblerView::isMouseEventInBlock(QMouseEvent* event)
{
    //Convert coordinates to system used in blocks
    int xofs = this->horizontalScrollBar()->value();
    int yofs = this->verticalScrollBar()->value();
    int x = event->x() + xofs + this->renderXOfs;
    int y = event->y() + yofs + this->renderYOfs;

    // Check each block for hits
    for(auto & blockIt : this->blocks)
    {
        DisassemblerBlock block = blockIt.second;
        //Compute coordinate relative to text area in block
        int blockx = x - (block.x + (2 * this->charWidth));
        int blocky = y - (block.y + (2 * this->charWidth));
        //Check to see if click is within bounds of block
        if((blockx < 0) || (blockx > (block.width - 4 * this->charWidth)))
            continue;
        if((blocky < 0) || (blocky > (block.height - 4 * this->charWidth)))
            continue;
        return true;
    }
    return false;
}

duint DisassemblerView::getInstrForMouseEvent(QMouseEvent* event)
{
    //Convert coordinates to system used in blocks
    int xofs = this->horizontalScrollBar()->value();
    int yofs = this->verticalScrollBar()->value();
    int x = event->x() + xofs - this->renderXOfs;
    int y = event->y() + yofs - this->renderYOfs;

    //Check each block for hits
    for(auto & blockIt : this->blocks)
    {
        auto block = blockIt.second;
        //Compute coordinate relative to text area in block
        int blockx = x - (block.x + (2 * this->charWidth));
        int blocky = y - (block.y + (2 * this->charWidth));
        //Check to see if click is within bounds of block
        if((blockx < 0) || (blockx > (block.width - 4 * this->charWidth)))
            continue;
        if((blocky < 0) || (blocky > (block.height - 4 * this->charWidth)))
            continue;
        //Compute row within text
        int row = int(blocky / this->charHeight);
        //Determine instruction for this row
        int cur_row = int(block.block.header_text.lines.size());
        if(row < cur_row)
            return block.block.entry;
        for(auto instr : block.block.instrs)
        {
            if(row < cur_row + int(instr.text.lines.size()))
                return instr.addr;
            cur_row += int(instr.text.lines.size());
        }
    }
    return 0;
}

bool DisassemblerView::getTokenForMouseEvent(QMouseEvent* event, Token & tokenOut)
{
    //Convert coordinates to system used in blocks
    int xofs = this->horizontalScrollBar()->value();
    int yofs = this->verticalScrollBar()->value();
    int x = event->x() + xofs - this->renderXOfs;
    int y = event->y() + yofs - this->renderYOfs;

    //Check each block for hits
    for(auto & blockIt : this->blocks)
    {
        auto block = blockIt.second;
        //Compute coordinate relative to text area in block
        int blockx = x - (block.x + (2 * this->charWidth));
        int blocky = y - (block.y + (2 * this->charWidth));
        //Check to see if click is within bounds of block
        if((blockx < 0) || (blockx > (block.width - 4 * this->charWidth)))
            continue;
        if((blocky < 0) || (blocky > (block.height - 4 * this->charWidth)))
            continue;
        //Compute row and column within text
        int col = int(blockx / this->charWidth);
        int row = int(blocky / this->charHeight);
        //Check tokens to see if one was clicked
        int cur_row = 0;
        for(auto line : block.block.header_text.tokens)
        {
            if(cur_row == row)
            {
                for(auto token : line)
                {
                    if((col >= token.start) && (col < (token.start + token.length)))
                    {
                        //Clicked on a token
                        tokenOut = token;
                        return true;
                    }
                }
            }
            cur_row += 1;
        }
        for(auto instr : block.block.instrs)
        {
            for(auto line : instr.text.tokens)
            {
                if(cur_row == row)
                {
                    for(auto token : line)
                    {
                        if((col >= token.start) && (col < (token.start + token.length)))
                        {
                            //Clicked on a token
                            tokenOut = token;
                            return true;
                        }
                    }
                }
                cur_row += 1;
            }
        }
    }
    return false;
}

bool DisassemblerView::find_instr(duint addr, Instr & instrOut)
{
    for(auto & blockIt : this->blocks)
        for(auto instr : blockIt.second.block.instrs)
            if(instr.addr == addr)
            {
                instrOut = instr;
                return true;
            }
    return false;
}

void DisassemblerView::mousePressEvent(QMouseEvent* event)
{
    if(event->button() != Qt::LeftButton && event->button() != Qt::RightButton)
        return;

    if(!this->isMouseEventInBlock(event))
    {
        //Click outside any block, enter scrolling mode
        this->scroll_base_x = event->x();
        this->scroll_base_y = event->y();
        this->scroll_mode = true;
        this->viewport()->grabMouse();
        return;
    }

    //Check for click on a token and highlight it
    Token token;
    if(this->getTokenForMouseEvent(event, token))
        this->highlight_token = HighlightToken::fromToken(token);
    else
        this->highlight_token = nullptr;

    //Update current instruction
    duint instr = this->getInstrForMouseEvent(event);
    if(instr != 0)
        this->cur_instr = instr;
    else
        this->cur_instr = 0;

    this->viewport()->update();

    /*if((instr != 0) && (event->button() == Qt::RightButton))
        this->context_menu(instr);*/
}

void DisassemblerView::mouseMoveEvent(QMouseEvent* event)
{
    if(this->scroll_mode)
    {
        int x_delta = this->scroll_base_x - event->x();
        int y_delta = this->scroll_base_y - event->y();
        this->scroll_base_x = event->x();
        this->scroll_base_y = event->y();
        this->horizontalScrollBar()->setValue(this->horizontalScrollBar()->value() + x_delta);
        this->verticalScrollBar()->setValue(this->verticalScrollBar()->value() + y_delta);
    }
}

void DisassemblerView::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->button() != Qt::LeftButton)
        return;

    if(this->scroll_mode)
    {
        this->scroll_mode = false;
        this->viewport()->releaseMouse();
    }
}

void DisassemblerView::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    //TODO
}

void DisassemblerView::prepareGraphNode(DisassemblerBlock & block)
{
    int width = 0;
    int height = 0;
    for(auto line : block.block.header_text.lines)
    {
        int chars = 0;
        for(auto part : line)
            chars += part.text.length();
        if(chars > width)
            width = chars;
        height += 1;
    }
    for(auto instr : block.block.instrs)
    {
        for(auto line : instr.text.lines)
        {
            int chars = 0;
            for(auto part : line)
                chars += part.text.length();
            if(chars > width)
                width = chars;
            height += 1;
        }
    }
    block.width = (width + 4) * this->charWidth + 4;
    block.height = (height * this->charHeight) + (4 * this->charWidth) + 4;
}

void DisassemblerView::adjustGraphLayout(DisassemblerBlock & block, int col, int row)
{
    block.col += col;
    block.row += row;
    for(auto edge : block.new_exits)
        this->adjustGraphLayout(this->blocks[edge], col, row);
}

void DisassemblerView::computeGraphLayout(DisassemblerBlock & block)
{
    //Compute child node layouts and arrange them horizontally
    int col = 0;
    int row_count = 1;
    for(auto edge : block.new_exits)
    {
        this->computeGraphLayout(this->blocks[edge]);
        this->adjustGraphLayout(this->blocks[edge], col, 1);
        col += this->blocks[edge].col_count;
        if((this->blocks[edge].row_count + 1) > row_count)
            row_count = this->blocks[edge].row_count + 1;
    }

    block.row = 0;
    if(col >= 2)
    {
        //Place this node centered over the child nodes
        block.col = (col - 2) / 2;
        block.col_count = col;
    }
    else
    {
        //No child nodes, set single node's width (nodes are 2 columns wide to allow
        //centering over a branch)
        block.col = 0;
        block.col_count = 2;
    }
    block.row_count = row_count;
}

bool DisassemblerView::isEdgeMarked(EdgesVector & edges, int row, int col, int index)
{
    if(index >= int(edges[row][col].size()))
        return false;
    return edges[row][col][index];
}

void DisassemblerView::markEdge(EdgesVector & edges, int row, int col, int index)
{
    while(int(edges[row][col].size()) <= index)
        edges[row][col].push_back(false);
    edges[row][col][index] = true;
}

int DisassemblerView::findHorizEdgeIndex(EdgesVector & edges, int row, int min_col, int max_col)
{
    //Find a valid index
    int i = 0;
    while(true)
    {
        bool valid = true;
        for(int col=min_col; col<max_col+1; col++) //TODO: use max_col+1 ?
            if(isEdgeMarked(edges, row, col, i))
            {
                valid = false;
                break;
            }
        if(valid)
            break;
        i++;
    }

    //Mark chosen index as used
    for(int col = min_col; col < max_col+1; col++) //TODO: use max_col+1 ?
        this->markEdge(edges, row, col, i);
    return i;
}

int DisassemblerView::findVertEdgeIndex(EdgesVector &edges, int col, int min_row, int max_row)
{
    //Find a valid index
    int i = 0;
    while(true)
    {
        bool valid = true;
        for(int row=min_row; row<max_row+1; row++) //TODO: use max_row+1 ?
            if(isEdgeMarked(edges, row, col, i))
            {
                valid = false;
                break;
            }
        if(valid)
            break;
        i++;
    }

    //Mark chosen index as used
    for(int row = min_row; row < max_row+1; row++) //TODO: use max_row+1 ?
        this->markEdge(edges, row, col, i);
    return i;
}

DisassemblerEdge DisassemblerView::routeEdge(EdgesVector & horiz_edges, EdgesVector & vert_edges, Matrix<bool> & edge_valid, DisassemblerBlock & start, DisassemblerBlock & end, QColor color)
{
    DisassemblerEdge edge;
    edge.color = color;
    edge.dest = &end;

    //Find edge index for initial outgoing line
    int i = 0;
    while(true)
    {
        if(!this->isEdgeMarked(vert_edges, start.row + 1, start.col + 1, i))
            break;
        i += 1;
    }
    this->markEdge(vert_edges, start.row + 1, start.col + 1, i);
    edge.addPoint(start.row + 1, start.col + 1);
    edge.start_index = i;
    bool horiz = false;

    //Find valid column for moving vertically to the target node
    int min_row, max_row;
    if(end.row < (start.row + 1))
    {
        min_row = end.row;
        max_row = start.row + 1;
    }
    else
    {
        min_row = start.row + 1;
        max_row = end.row;
    }
    int col = start.col + 1;
    if(min_row != max_row)
    {
        int ofs = 0;
        while(true)
        {
            col = start.col + 1 - ofs;
            if(col >= 0)
            {
                bool valid = true;
                for(int row = min_row; row < max_row + 1; row++)
                {
                    if(!edge_valid[row][col])
                    {
                        valid = false;
                        break;
                    }
                }
                if(valid)
                    break;
            }

            col = start.col + 1 + ofs;
            if(col < int(edge_valid[min_row].size()))
            {
                bool valid = true;
                for(int row = min_row; row < max_row + 1; row++)
                {
                    if(!edge_valid[row][col])
                    {
                        valid = false;
                        break;
                    }
                }
                if(valid)
                    break;
            }

            ofs += 1;
        }
    }

    if(col != (start.col + 1))
    {
        //Not in same column, need to generate a line for moving to the correct column
        int min_col, max_col;
        if(col < (start.col + 1))
        {
            min_col = col;
            max_col = start.col + 1;
        }
        else
        {
            min_col = start.col + 1;
            max_col = col;
        }
        int index = this->findHorizEdgeIndex(horiz_edges, start.row + 1, min_col, max_col);
        edge.addPoint(start.row + 1, col, index);
        horiz = true;
    }

    if(end.row != (start.row + 1))
    {
        //Not in same row, need to generate a line for moving to the correct row
        int index = this->findVertEdgeIndex(vert_edges, col, min_row, max_row);
        edge.addPoint(end.row, col, index);
        horiz = false;
    }

    if(col != (end.col + 1))
    {
        //Not in ending column, need to generate a line for moving to the correct column
        int min_col, max_col;
        if(col < (end.col + 1))
        {
            min_col = col;
            max_col = end.col + 1;
        }
        else
        {
            min_col = end.col + 1;
            max_col = col;
        }
        int index = this->findHorizEdgeIndex(horiz_edges, end.row, min_col, max_col);
        edge.addPoint(end.row, end.col + 1, index);
        horiz = true;
    }

    //If last line was horizontal, choose the ending edge index for the incoming edge
    if(horiz)
    {
        int index = this->findVertEdgeIndex(vert_edges, end.col + 1, end.row, end.row);
        edge.points[int(edge.points.size()) - 1].index = index;
    }

    return edge;
}

template<class T>
static void removeFromVec(std::vector<T> & vec, T elem)
{
    vec.erase(std::remove(vec.begin(), vec.end(), elem), vec.end());
}

template<class T>
static void initVec(std::vector<T> & vec, size_t size, T value)
{
    vec.resize(size);
    for(size_t i = 0; i < size; i++)
        vec[i] = value;
}

void DisassemblerView::renderFunction(Function & func)
{
    //Create render nodes
    this->blocks.clear();
    for(auto & block : func.blocks)
    {
        this->blocks[block.entry] = DisassemblerBlock(block);
        this->prepareGraphNode(this->blocks[block.entry]);
    }

    //Populate incoming lists
    for(auto & blockIt : this->blocks)
    {
        auto & block = blockIt.second;
        for(auto & edge : block.block.exits)
            this->blocks[edge].incoming.push_back(block.block.entry);
    }

    //Construct acyclic graph where each node is used as an edge exactly once
    //auto block = func.blocks[func.entry];
    std::unordered_set<duint> visited;
    visited.insert(func.entry);
    std::queue<DisassemblerBlock> queue;
    queue.push(this->blocks[func.entry]);
    auto changed = true;

    while(changed)
    {
        changed = false;

        //First pick nodes that have single entry points
        while(!queue.empty())
        {
            auto block = queue.front();
            queue.pop();

            for(auto & edge : block.block.exits)
            {
                if(visited.count(edge))
                    continue;

                //If node has no more unseen incoming edges, add it to the graph layout now
                if(int(this->blocks[edge].incoming.size()) == 1)
                {
                    removeFromVec(this->blocks[edge].incoming, block.block.entry);
                    block.new_exits.push_back(edge);
                    queue.push(this->blocks[edge]);
                    visited.insert(edge);
                    changed = true;
                }
            }
        }

        //No more nodes satisfy constraints, pick a node to continue constructing the graph
        duint best = 0;
        int best_edges;
        DisassemblerBlock best_parent;
        for(auto & blockIt : this->blocks)
        {
            auto & block = blockIt.second;
            if(!visited.count(block.block.entry))
                continue;
            for(auto & edge : block.block.exits)
            {
                if(visited.count(edge))
                    continue;
                if((best == 0) || (int(this->blocks[edge].incoming.size()) < best_edges) || (
                            (int(this->blocks[edge].incoming.size()) == best_edges) && (edge < best)))
                {
                    best = edge;
                    best_edges = int(this->blocks[edge].incoming.size());
                    best_parent = block;
                }
            }
        }

        if(best != 0)
        {
            removeFromVec(this->blocks[best].incoming, best_parent.block.entry);
            best_parent.new_exits.push_back(best);
            visited.insert(best);
            changed = true;
        }
    }

    //Compute graph layout from bottom up
    this->computeGraphLayout(this->blocks[func.entry]);

    //Prepare edge routing
    EdgesVector horiz_edges, vert_edges;
    horiz_edges.resize(this->blocks[func.entry].row_count + 1);
    vert_edges.resize(this->blocks[func.entry].row_count + 1);
    Matrix<bool> edge_valid;
    edge_valid.resize(this->blocks[func.entry].row_count + 1);
    for(int row = 0; row < this->blocks[func.entry].row_count + 1; row++)
    {
        horiz_edges[row].resize(this->blocks[func.entry].col_count + 1);
        vert_edges[row].resize(this->blocks[func.entry].col_count + 1);
        edge_valid[row].resize(this->blocks[func.entry].col_count + 1);
        for(int col = 0; col < this->blocks[func.entry].col_count + 1; col++)
        {
            horiz_edges[row][col].clear();
            vert_edges[row][col].clear();
        }
    }
    for(auto & blockIt : this->blocks)
    {
        auto & block = blockIt.second;
        edge_valid[block.row][block.col + 1] = false;
    }

    //Perform edge routing
    for(auto & blockIt : this->blocks)
    {
        auto & block = blockIt.second;
        auto start = block;
        for(auto & edge : block.block.exits)
        {
            auto end = this->blocks[edge];
            QColor color(Qt::black);
            if(edge == block.block.true_path)
                color = QColor(0, 144, 0);
            else if(edge == block.block.false_path)
                color = QColor(144, 0, 0);
            start.edges.push_back(this->routeEdge(horiz_edges, vert_edges, edge_valid, start, end, color));
        }
    }

    //Compute edge counts for each row and column
    std::vector<int> col_edge_count, row_edge_count;
    initVec(col_edge_count, this->blocks[func.entry].col_count + 1, 0);
    initVec(row_edge_count, this->blocks[func.entry].row_count + 1, 0);
    for(int row = 0; row < this->blocks[func.entry].row_count + 1; row++)
    {
        for(int col = 0; col < this->blocks[func.entry].col_count + 1; col++)
        {
            if(int(horiz_edges[row][col].size()) > row_edge_count[row])
                row_edge_count[row] = int(horiz_edges[row][col].size());
            if(int(vert_edges[row][col].size()) > col_edge_count[col])
                col_edge_count[col] = int(vert_edges[row][col].size());
        }
    }

    //Compute row and column sizes
    std::vector<int> col_width, row_height;
    initVec(col_width, this->blocks[func.entry].col_count + 1, 0);
    initVec(row_height, this->blocks[func.entry].row_count + 1, 0);
    for(auto & blockIt : this->blocks)
    {
        auto & block = blockIt.second;
        if((int(block.width / 2)) > col_width[block.col])
            col_width[block.col] = int(block.width / 2);
        if((int(block.width / 2)) > col_width[block.col + 1])
            col_width[block.col + 1] = int(block.width / 2);
        if(int(block.height) > row_height[block.row])
            row_height[block.row] = int(block.height);
    }

    //Compute row and column positions
    std::vector<int> col_x, row_y;
    initVec(col_x, this->blocks[func.entry].col_count, 0);
    initVec(row_y, this->blocks[func.entry].row_count, 0);
    initVec(this->col_edge_x, this->blocks[func.entry].col_count + 1, 0);
    initVec(this->row_edge_y, this->blocks[func.entry].row_count + 1, 0);
    int x = 16;
    for(int i = 0; i < this->blocks[func.entry].col_count; i++)
    {
        this->col_edge_x[i] = x;
        x += 8 * col_edge_count[i];
        col_x[i] = x;
        x += col_width[i];
    }
    int y = 16;
    for(int i = 0; i < this->blocks[func.entry].row_count; i++)
    {
        this->row_edge_y[i] = y;
        y += 8 * row_edge_count[i];
        row_y[i] = y;
        y += row_height[i];
    }
    this->col_edge_x[this->blocks[func.entry].col_count] = x;
    this->row_edge_y[this->blocks[func.entry].row_count] = y;
    this->width = x + 16 + (8 * col_edge_count[this->blocks[func.entry].col_count]);
    this->height = y + 16 + (8 * row_edge_count[this->blocks[func.entry].row_count]);

    //Compute node positions
    for(auto & blockIt : this->blocks)
    {
        auto & block = blockIt.second;
        block.x = int(
                    (col_x[block.col] + col_width[block.col] + 4 * col_edge_count[block.col + 1]) - (block.width / 2));
        if((block.x + block.width) > (
                    col_x[block.col] + col_width[block.col] + col_width[block.col + 1] + 8 * col_edge_count[
                    block.col + 1]))
        {
            block.x = int((col_x[block.col] + col_width[block.col] + col_width[block.col + 1] + 8 * col_edge_count[
                    block.col + 1]) - block.width);
        }
        block.y = row_y[block.row];
    }

    //Precompute coordinates for edges
    for(auto & blockIt : this->blocks)
    {
        auto & block = blockIt.second;
        for(auto & edge : block.edges)
        {
            auto start = edge.points[0];
            //auto start_row = start.row; //TODO: bug?
            auto start_col = start.col;
            auto last_index = edge.start_index;
            auto last_pt = QPoint(this->col_edge_x[start_col] + (8 * last_index) + 4,
                                  block.y + block.height + 4 - (2 * this->charWidth));
            QPolygonF pts;
            pts.append(last_pt);

            for(int i = 0; i < int(edge.points.size()); i++)
            {
                auto end = edge.points[i];
                auto end_row = end.row;
                auto end_col = end.col;
                auto last_index = end.index;
                QPoint new_pt;
                if(start_col == end_col)
                    new_pt = QPoint(last_pt.x(), this->row_edge_y[end_row] + (8 * last_index) + 4);
                else
                    new_pt = QPoint(this->col_edge_x[end_col] + (8 * last_index) + 4, last_pt.y());
                pts.push_back(new_pt);
                last_pt = new_pt;
                start_col = end_col;
            }

            auto new_pt = QPoint(last_pt.x(), edge.dest->y + this->charWidth - 1);
            pts.push_back(new_pt);
            edge.polyline = pts;

            pts.clear();
            pts.append(QPoint(new_pt.x() - 3, new_pt.y() - 6));
            pts.append(QPoint(new_pt.x() + 3, new_pt.y() - 6));
            pts.append(new_pt);
            edge.arrow = pts;
        }
    }

    //Adjust scroll bars for new size
    auto areaSize = this->viewport()->size();
    this->adjustSize(areaSize.width(), areaSize.height());

    if(this->desired_pos)
    {
        //There was a position saved, navigate to it
        this->horizontalScrollBar()->setValue(this->desired_pos[0]);
        this->verticalScrollBar()->setValue(this->desired_pos[1]);
    }
    else if(this->cur_instr != 0)
        this->show_cur_instr();
    else
    {
        //Ensure start node is visible
        auto start_x = this->blocks[func.entry].x + this->renderXOfs + int(this->blocks[func.entry].width / 2);
        this->horizontalScrollBar()->setValue(start_x - int(areaSize.width() / 2));
        this->verticalScrollBar()->setValue(0);
    }

    this->update_id = func.update_id;
    this->ready = true;
    this->viewport()->update(0, 0, areaSize.width(), areaSize.height());
}

void DisassemblerView::show_cur_instr()
{
    for(auto & blockIt : this->blocks)
    {
        auto & block = blockIt.second;
        auto row = int(block.block.header_text.lines.size());
        for(auto & instr : block.block.instrs)
        {
            if(this->cur_instr == instr.addr)
            {
                auto x = block.x + int(block.width / 2);
                auto y = block.y + (2 * this->charWidth) + int((row + 0.5) * this->charHeight);
                this->horizontalScrollBar()->setValue(x + this->renderXOfs -
                                                      int(this->horizontalScrollBar()->pageStep() / 2));
                this->verticalScrollBar()->setValue(y + this->renderYOfs -
                                                    int(this->verticalScrollBar()->pageStep() / 2));
                return;
            }
            row += int(instr.text.lines.size());
        }
    }
}

void DisassemblerView::updateTimerEvent()
{

}
