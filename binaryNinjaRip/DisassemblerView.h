#ifndef DISASSEMBLERVIEW_H
#define DISASSEMBLERVIEW_H

#include <QObject>
#include <QWidget>
#include <QAbstractScrollArea>
#include <QPaintEvent>
#include <QTimer>
#include <QSize>
#include <QResizeEvent>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <algorithm>

typedef size_t duint;

struct View
{
    //dummy class
};

struct Data
{
    duint entry;

    bool write(const Data & other)
    {
        Q_UNUSED(other);
        return false;
    }

    //dummy class
};

struct Analysis
{
    Data & data;

    Analysis(Data & data)
        : data(data) {}
    //dummy class
};

struct DisassemblerBlock;

struct Point
{
    int row; //point[0]
    int col; //point[1]
    int index; //point[2]
};

struct DisassemblerEdge
{
    QColor color;
    DisassemblerBlock* dest;
    std::vector<Point> points;
    int start_index = 0;

    QPolygonF polyline;
    QPolygonF arrow;

    void addPoint(int row, int col, int index = 0)
    {
        Point point;
        point.row = row;
        point.col = col;
        point.index = 0;
        if(int(this->points.size()) > 1)
            this->points[this->points.size() - 2].index = index;
    }
};

struct Token
{
    int start; //token[0]
    int length; //token[1]
    QString type; //token[2]
    duint addr; //token[3]
    QString name; //token[4]
};

struct HighlightToken
{
    QString type; //highlight_token[0]
    duint addr; //highlight_token[1]
    QString name; //highlight_token[2]

    bool equalsToken(const Token & token)
    {
        return this->type == token.type &&
                this->addr == token.addr &&
                this->name == token.name;
    }

    static HighlightToken* fromToken(const Token & token)
    {
        auto result = new HighlightToken();
        result->type = token.type;
        result->addr = token.addr;
        result->name = token.name;
        return result;
    }
};

struct Line
{
    QString text; //line[0]
    QColor color; //line[1]
};

struct Text
{
    std::vector<std::vector<Line>> lines;
    std::vector<std::vector<Token>> tokens;
};

struct Instr
{
    duint addr = 0;
    Text text;
};

struct Block
{
    Text header_text;
    std::vector<Instr> instrs;
    std::vector<duint> exits;
    duint entry = 0;
    duint true_path = 0;
    duint false_path = 0;
};

struct DisassemblerBlock
{
    DisassemblerBlock() {}
    DisassemblerBlock(Block & block)
        : block(block) {}

    Block block;
    std::vector<DisassemblerEdge> edges;
    std::vector<duint> incoming;
    std::vector<duint> new_exits;
    std::vector<DisassemblerEdge> exits;
    DisassemblerEdge true_path;
    DisassemblerEdge false_path;
    qreal x = 0.0;
    qreal y = 0.0;
    int width = 0;
    int height = 0;
    int col = 0;
    int col_count = 0;
    int row = 0;
    int row_count = 0;
};

struct Function
{
    duint entry;
    duint update_id;
    std::vector<Block> blocks;
};

class DisassemblerView : public QAbstractScrollArea
{
    Q_OBJECT
public:
    DisassemblerView(QWidget* parent, const Data & data, const View & view);
    void initFont();
    void adjustSize(int width, int height);
    void resizeEvent(QResizeEvent* event);
    duint get_cursor_pos();
    void set_cursor_pos(duint addr);
    std::tuple<duint, duint> get_selection_range();
    void set_selection_range(std::tuple<duint, duint> range);
    bool write(const Data & data);
    void copy_address();
    //void analysis_thread_proc();
    //void closeRequest();
    void paintEvent(QPaintEvent* event);
    bool isMouseEventInBlock(QMouseEvent* event);
    duint getInstrForMouseEvent(QMouseEvent* event);
    bool getTokenForMouseEvent(QMouseEvent* event, Token & token);
    bool find_instr(duint addr, Instr & instr);
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseDoubleClickEvent(QMouseEvent* event);
    void prepareGraphNode(DisassemblerBlock & block);
    void adjustGraphLayout(DisassemblerBlock & block, int col, int row);
    void computeGraphLayout(DisassemblerBlock & block);
    template<typename T>
    using Matrix = std::vector<std::vector<T>>;
    using EdgesVector = Matrix<std::vector<bool>>;
    bool isEdgeMarked(EdgesVector & edges, int row, int col, int index);
    void markEdge(EdgesVector & edges, int row, int col, int index);
    int findHorizEdgeIndex(EdgesVector & edges, int row, int min_col, int max_col);
    int findVertEdgeIndex(EdgesVector & edges, int col, int min_row, int max_row);
    DisassemblerEdge routeEdge(EdgesVector & horiz_edges, EdgesVector & vert_edges, Matrix<bool> & edge_valid, DisassemblerBlock & start, DisassemblerBlock & end, QColor color);
    void renderFunction(Function & func);
    void show_cur_instr();

public slots:
    void updateTimerEvent();

private:
    QString status;
    View view;
    Data data;
    Analysis analysis;
    duint function;
    QTimer* updateTimer;
    int baseline;
    qreal charWidth;
    int charHeight;
    int charOffset;
    int width;
    int height;
    int renderWidth;
    int renderHeight;
    int renderXOfs;
    int renderYOfs;
    duint cur_instr;
    int scroll_base_x;
    int scroll_base_y;
    duint update_id;
    bool scroll_mode;
    bool ready;
    int* desired_pos;
    std::unordered_map<duint, DisassemblerBlock> blocks;
    HighlightToken* highlight_token;
    std::vector<int> col_edge_x;
    std::vector<int> row_edge_y;
};

#endif // DISASSEMBLERVIEW_H
