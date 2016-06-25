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

typedef size_t duint;

struct View
{
    //dummy class
};

struct Data
{
    duint entry;
    //dummy class
};

struct Analysis
{
    Data & data;

    Analysis(Data & data)
        : data(data) {}
    //dummy class
};

struct DisassemblerEdge
{
    QColor color;
    QPointF dest;
    std::vector<QPointF> points;
    int start_index = 0;

    QPolygonF polyline;
    QPolygonF arrow;
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
    duint entry = 0;
};

struct DisassemberBlock
{
    Block block;
    std::vector<DisassemblerEdge> edges;
    std::vector<DisassemblerEdge> incoming;
    std::vector<DisassemblerEdge> new_exits;
    std::vector<DisassemblerEdge> exits;
    DisassemblerEdge true_path;
    DisassemblerEdge false_path;
    qreal x = 0.0;
    qreal y = 0.0;
    int width = 0;
    int height = 0;
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
    //void closeRequest();
    void paintEvent(QPaintEvent* event);

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
    QSize areaSize;
    int width;
    int height;
    int renderWidth;
    int renderHeight;
    int renderXOfs;
    int renderYOfs;
    duint cur_instr;
    bool ready;
    std::unordered_map<duint, DisassemberBlock> blocks;
    HighlightToken* highlight_token;
};

#endif // DISASSEMBLERVIEW_H
