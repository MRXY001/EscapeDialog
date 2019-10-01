#include "escapedialog.h"

EscapeDialog::EscapeDialog(QString title, QString msg, QString esc, QString nor, QWidget *parent)
    : QDialog(parent), mt(rd()),
      exchanged(false), escape_count(0), last_escape_index(0), has_overlapped(false)
{
    setWindowTitle(title);

    QVBoxLayout* main_vlayout = new QVBoxLayout;
    msg_lab = new QLabel(msg, this);
    esc_btn = new HoverButton(esc, this);
    nor_btn = new HoverButton(nor, this);
    main_vlayout->addWidget(msg_lab);
    main_vlayout->addSpacing(esc_btn->height() + MARGIN*2);
    main_vlayout->setMargin(MARGIN);
    setLayout(main_vlayout);

    setMinimumSize(qMax(msg_lab->width()+MARGIN*2, esc_btn->width()+nor_btn->width()+MARGIN*4),
                   msg_lab->height()+esc_btn->height()+MARGIN*3);
    int btn_w = qMax(esc_btn->width(), nor_btn->width());
    esc_btn->setFixedWidth(btn_w);
    nor_btn->setFixedWidth(btn_w);

    resetBtnPos();
    nor_btn->setFocus();

    connect(nor_btn, &HoverButton::clicked, [=]{ // 接受（不进行任何操作）
        if (!exchanged)
            this->accept();
        else
            this->reject();
    });
    connect(esc_btn, &HoverButton::clicked, [=]{ // 拒绝（点不到的操作）
        if (!exchanged)
            this->reject();
        else
            this->accept();
    });

    connect(esc_btn, SIGNAL(signalEntered(QPoint)), this, SLOT(slotPosEntered(QPoint))); // 进入按钮（移动按钮或者交换位置）

    connect(esc_btn, &HoverButton::signalLeaved, [=](QPoint){ // 离开按钮（如果两个按钮互换了，换回来）
        if (exchanged)
            slotExchangeButton();
    });
}

void EscapeDialog::resizeEvent(QResizeEvent *event)
{
    resetBtnPos();
    return QDialog::resizeEvent(event);
}

void EscapeDialog::resetBtnPos()
{
    int w = width(), h = height();
    nor_btn->move(w-MARGIN-nor_btn->width(), h-MARGIN-nor_btn->height());
    esc_btn->move(nor_btn->geometry().left()-MARGIN-esc_btn->width(), nor_btn->geometry().top());
}

qint64 EscapeDialog::getTimestamp()
{
    return QDateTime::currentDateTime().toMSecsSinceEpoch();
}

int EscapeDialog::getRandom(int min, int max)
{
#if defined (Q_OS_WIN) || defined(Q_OS_LINUX)
    return static_cast<int>(mt() % static_cast<unsigned long>((max-min+1))) + min;
#elif defined(Q_OS_MAC)

#else
    return rand() % (max-min+1) + min;
#endif
}

bool EscapeDialog::isEqual(int a, int b)
{
    return abs(a-b) <= 2;
}

/**
 * 按钮触发鼠标进入事件
 * 移动该按钮，或者和另一个按钮进行交换文字
 */
void EscapeDialog::slotPosEntered(QPoint point)
{
    // 判断移动按钮还是交换按钮
    bool is_exchanged = false;
    if (escape_count > 10 && escape_count - last_escape_index > 5 && getRandom(1, 20)==1)
        is_exchanged = true;

    // 移动按钮
    if (!is_exchanged)
    {
        slotEscapeButton(point);
    }
    // 交换按钮
    else
    {
        slotExchangeButton();
    }

    escape_count++;
}

/**
 * 移动某个按钮的位置
 */
void EscapeDialog::slotEscapeButton(QPoint p)
{
    QPoint aim_pos(esc_btn->pos()); // 移动的目标点的位置
    QRect geo = esc_btn->geometry();
    qDebug() << "escape_count" << escape_count;

    // 前三次，只在一定范围内移动
    if (escape_count <= 3)
    {
        if (geo.top() == nor_btn->geometry().top()
                && abs(geo.right()+MARGIN - nor_btn->geometry().left())<=2) // 还在原来的位置
        {
            // 根据进入位置，判断移动的方向
            if (isEqual(p.x(), geo.width()) || isEqual(p.y(), 0)) // 右边或上边进入，左移
            {
                aim_pos = esc_btn->pos() + QPoint(-MARGIN-esc_btn->width(), 0);
            }
            else // 其余方向：上移
            {
                aim_pos = esc_btn->pos() + QPoint(0, -MARGIN-esc_btn->height());
            }
        }
        else // 恢复到原来的位置
        {
            aim_pos = nor_btn->pos() + QPoint(-MARGIN-esc_btn->width(), 0);
        }
    }
    else if (escape_count % 100 == 50) // 50次，直接躲到另一个按钮下面（需要用按键才能解决）
    {
        // 注意：如果两个按钮大小不一样则没什么意义了（普通按钮小的话，鼠标点得到）
        aim_pos = nor_btn->pos();
        QString text = nor_btn->text();
        nor_btn->setText("在我下面");
        QTimer::singleShot(3000, [=]{
            nor_btn->setText("你点不到");
        });
        QTimer::singleShot(6000, [=]{
            nor_btn->setText(text);
        });
        QTimer::singleShot(getRandom(6000, 12000), [=]{
            last_escape_index = escape_count;
            slotEscapeButton(QPoint());
        });
    }
    // 后面就随便跑啦
    else
    {
        do{
            aim_pos = QPoint(getRandom(-geo.width()/2, width()-geo.width()/2),
                             getRandom(-geo.height()/2, height()-geo.height()/2));
        } while ( // 如果在原来的范围内，或者鼠标点到的地方，则重新判断
                 (aim_pos.x()>=geo.left() && aim_pos.x()<=geo.right())
              || (aim_pos.y()>=geo.top() && aim_pos.y()<=geo.bottom())
              || QRect(aim_pos.x(), aim_pos.y(), geo.width(), geo.height())
                  .contains(mapFromGlobal(QCursor::pos()), false)
                 );
    }

    QPropertyAnimation* ani = new QPropertyAnimation(esc_btn, "pos");
    ani->setStartValue(esc_btn->pos());
    ani->setEndValue(aim_pos);
    ani->setEasingCurve(QEasingCurve::OutQuint);
    ani->setDuration(100 + static_cast<int>(sqrt((esc_btn->pos()-aim_pos).manhattanLength())));
    ani->start();
    connect(ani, SIGNAL(finished()), ani, SLOT(deleteLater()));
}

void EscapeDialog::slotExchangeButton()
{
    QString text1 = esc_btn->text();
    QString text2 = nor_btn->text();
    esc_btn->setText(text2);
    nor_btn->setText(text1);
    exchanged = !exchanged;
    last_escape_index = escape_count;
}
