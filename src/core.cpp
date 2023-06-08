#include <X11/Xlib.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <xviewmap.hpp>

namespace XViewMap
{
// コンストラクタ、スレッド
ViewMap::ViewMap()
{
    // ほぼ https://github.com/QMonkey/Xlib-demo/blob/master/src/simple-drawing.c
    // のコピペ

    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        std::cerr << "[XViewMap] Failed to create display" << std::endl;
        return;
    }
    v_display = static_cast<void*>(display);

    screen_num = DefaultScreen(display);
    // フルスクリーンのサイズ?
    int dwidth = DisplayWidth(display, screen_num);
    int dheight = DisplayHeight(display, screen_num);
    // 画面位置とサイズ
    int winx = 0, winy = 0;
    win_width = dwidth * 2 / 3;
    win_height = dheight * 2 / 3;
    int win_border_width = 2;
    black_pixel = BlackPixel(display, screen_num);
    white_pixel = WhitePixel(display, screen_num);
    win = XCreateSimpleWindow(display, RootWindow(display, screen_num), winx, winy, win_width,
        win_height, win_border_width, black_pixel, white_pixel);

    // 画面を表示
    XMapWindow(display, win);
    XStoreName(display, win, "XViewMap");
    flush();

    // マウス入力を有効にする
    XSelectInput(display, win,
        ButtonMotionMask | ButtonPressMask | ButtonReleaseMask | StructureNotifyMask
            | ExposureMask);

    XGCValues values;
    GC gc = XCreateGC(display, win, 0, &values);
    v_gc = gc;
    // if (gc < 0) {
    //     std::cerr << "[XViewMap] Failed to create gc" << std::endl;
    //     return;
    // }
    XSetBackground(display, gc, white_pixel);

    int line_style = LineSolid;
    int cap_style = CapButt;
    int join_style = JoinBevel;
    int line_width = 2;
    XSetLineAttributes(display, gc, line_width, line_style, cap_style, join_style);
    XSetFillStyle(display, gc, FillSolid);

    Colormap screen_colormap = DefaultColormap(display, DefaultScreen(display));
    XColor c;
#define XColorDef(col)                                        \
    XAllocNamedColor(display, screen_colormap, #col, &c, &c); \
    col##_pixel = c.pixel;
    XColorDef(red);
    XColorDef(orange);
    XColorDef(forestgreen);
    XColorDef(blue);
#undef XColorDef

    setField(-3000, -3000, 3000, 3000);  // 仮で適当なサイズのフィールドを設定

    win_thread = std::make_optional<std::thread>([this]() { winThread(); });
    pos_thread
        = std::make_optional<std::thread>([this]() { posThread(pos_history, orange_pixel); });
    locus_thread
        = std::make_optional<std::thread>([this]() { posThread(locus_history, blue_pixel); });
}

ViewMap::~ViewMap()
{
    if (v_display) {
        std::lock_guard lock(x11_mutex);
        Display* display = static_cast<Display*>(*v_display);
        v_display = std::nullopt;
        XCloseDisplay(display);
    }
    pos_history.terminate();
    locus_history.terminate();
    pos_thread->join();
    locus_thread->join();
    win_thread->join();
}

void ViewMap::winThread()
{
    while (v_display) {
        Display* display = static_cast<Display*>(*v_display);

        static int mouse_last_x, mouse_last_y;
        static bool mouse_last_moved = false;
        {
            std::lock_guard lock(x11_mutex);
            while (XPending(display)) {
                XEvent ev;
                XNextEvent(display, &ev);
                switch (ev.type) {
                case Expose:
                    updateWindow();
                    break;
                case ConfigureNotify:  // 画面サイズが変わったとき
                    if (win_width != ev.xconfigure.width || win_height != ev.xconfigure.height) {
                        win_width = ev.xconfigure.width;
                        win_height = ev.xconfigure.height;
                    }
                    break;
                case MotionNotify:  // マウスドラッグ
                    if (mouse_last_moved) {
                        field_ofs_x += mouse_last_x - ev.xmotion.x;
                        field_ofs_y += mouse_last_y - ev.xmotion.y;
                        updateWindow();
                    }
                    mouse_last_x = ev.xmotion.x;
                    mouse_last_y = ev.xmotion.y;
                    mouse_last_moved = true;
                    break;
                case ButtonRelease:
                    mouse_last_moved = false;
                    break;
                case ButtonPress:  // スクロール
                    constexpr double zoom_rate = 1.1;
                    if (ev.xbutton.button == 4
                        && zoom * zoom_rate < win_height / field_height * 3) {
                        zoom *= zoom_rate;
                        field_ofs_x = static_cast<int>(
                            round(field_ofs_x * zoom_rate + ev.xbutton.x * (zoom_rate - 1)));
                        field_ofs_y = static_cast<int>(
                            round(field_ofs_y * zoom_rate + ev.xbutton.y * (zoom_rate - 1)));
                        resetPixmap();
                        updateWindow();
                    }
                    if (ev.xbutton.button == 5
                        && zoom / zoom_rate > win_height / field_height / 3) {
                        zoom /= zoom_rate;
                        field_ofs_x = static_cast<int>(
                            round(field_ofs_x / zoom_rate - ev.xbutton.x * (zoom_rate - 1)));
                        field_ofs_y = static_cast<int>(
                            round(field_ofs_y / zoom_rate - ev.xbutton.y * (zoom_rate - 1)));
                        resetPixmap();
                        updateWindow();
                    }
                    break;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void ViewMap::posThread(PositionHistory& history, unsigned long pixel)
{
    while (v_display) {
        // 軌跡を描画
        auto [last, next] = history.waitPopLastNext();
        // lastはoptional<Pos>, nextはPos
        if (!last || next != *last) {
            std::lock_guard lock(x11_mutex);
            if (last) {
                drawFieldLine_impl(next, *last, pixel);
            }
            updateWindow();
            // flush();
        }
    }
}

void ViewMap::updatePos(const Pos& pos, const Pos& vel)
{
    pos_history.push(pos);
    this->vel_x = vel.x;
    this->vel_y = vel.y;
    // this->omega = vel.th;
}
void ViewMap::resetPos(const Pos& pos)
{
    pos_history.reset(pos);
}
void ViewMap::updateLocus(const Pos& pos)
{
    locus_history.push(pos);
}
void ViewMap::resetLocus(const Pos& pos)
{
    locus_history.reset(pos);
}

void ViewMap::setField(double min_x, double min_y, double max_x, double max_y)
{
    field_min_x = min_x;
    field_min_y = min_y;
    field_max_x = max_x;
    field_max_y = max_y;
    resetFieldZoom();
    {
        std::lock_guard lock(x11_mutex);
        resetPixmap();
        updateWindow();
        // flush();
    }
}
void ViewMap::drawFieldLine(double x1, double y1, double x2, double y2)
{
    LineData ld{{x1, y1}, {x2, y2}};
    field_lines.push_back(ld);
    {
        std::lock_guard lock(x11_mutex);
        drawFieldLine_impl(ld, black_pixel);
        updateWindow();
        // flush();
    }
}
void ViewMap::drawFieldArc(double x, double y, double r, double a1, double a2)
{
    ArcData ad{x, y, r, a1, a2};
    field_arcs.push_back(ad);
    {
        std::lock_guard lock(x11_mutex);
        drawFieldArc_impl(ad, black_pixel);
        updateWindow();
        // flush();
    }
}

// private

void ViewMap::resetFieldZoom()
{
    // フィールド幅と高さから拡大率と位置を調整
    field_width = field_max_y - field_min_y;
    field_height = field_max_x - field_min_x;
    if (win_width / field_width > win_height / field_height) {
        zoom = win_height / field_height;
        // win_width = field_width * zoom;
        field_ofs_x = static_cast<int>(round(-(win_width - field_width * zoom) / 2));
    } else {
        zoom = win_width / field_width;
        // win_height = field_height * zoom;
        field_ofs_y = static_cast<int>(round(-(win_height - field_height * zoom) / 2));
    }
}
int ViewMap::xFieldToWindow(double x)
{
    return static_cast<int>(round((field_max_x - x) * zoom));
}
int ViewMap::yFieldToWindow(double y)
{
    return static_cast<int>(round((field_max_y - y) * zoom));
}

void ViewMap::flush()
{
    if (v_display) {
        Display* display = static_cast<Display*>(*v_display);
        XFlush(display);
    }
}
void ViewMap::updateWindow()
{
    if (v_display) {
        Display* display = static_cast<Display*>(*v_display);
        GC gc = static_cast<GC>(v_gc);

        // ロボット無い状態のフィールドを画面にコピー
        XCopyArea(
            display, *field_p, win, gc, field_ofs_x, field_ofs_y, win_width, win_height, 0, 0);

        auto pos = pos_history.getNow();
        if (pos) {
            // ロボットの外形描画
            for (std::size_t i = 0; i < machine.size(); i++) {
                drawWinLine_impl(
                    *pos + machine[i], *pos + machine[(i + 1) % machine.size()], red_pixel);
            }
            // オムニ描画
            for (const auto& wheel : wheels) {
                drawWinLine_impl((*pos + wheel).x - wheel_radius * cos(pos->th + wheel.th),
                    (*pos + wheel).y - wheel_radius * sin(pos->th + wheel.th),
                    (*pos + wheel).x + wheel_radius * cos(pos->th + wheel.th),
                    (*pos + wheel).y + wheel_radius * sin(pos->th + wheel.th), red_pixel);
            }

            // 速度ベクトル描画
            drawWinLine_impl(pos->x, pos->y, pos->x + vel_x, pos->y + vel_y, forestgreen_pixel);
            /*
            double vel_angle = atan2(vel_y, vel_x);
            // 矢印の先っぽ
            drawWinLine_impl(
                pos->x + vel_x - 0.3 * cos(vel_angle + 3.14 / 6),
                pos->y + vel_y - 0.3 * sin(vel_angle + 3.14 / 6),
                pos->x + vel_x,
                pos->y + vel_y,
                forestgreen_pixel);
            drawWinLine_impl(
                pos->x + vel_x - 0.3 * cos(vel_angle - 3.14 / 6),
                pos->y + vel_y - 0.3 * sin(vel_angle - 3.14 / 6),
                pos->x + vel_x,
                pos->y + vel_y,
                forestgreen_pixel);
            */
        }

        // flush();
    }
}

void ViewMap::resetPixmap()
{
    if (v_display) {
        Display* display = static_cast<Display*>(*v_display);
        GC gc = static_cast<GC>(v_gc);

        if (field_p) {
            XFreePixmap(display, *field_p);
        }
        int pm_width = static_cast<int>(round(field_width * zoom));
        int pm_height = static_cast<int>(round(field_height * zoom));

        field_p
            = XCreatePixmap(display, win, pm_width, pm_height, DefaultDepth(display, screen_num));
        flush();

        XSetForeground(display, gc, white_pixel);
        XFillRectangle(display, *field_p, gc, 0, 0, pm_width, pm_height);

        // フィールド外枠
        XPoint field_outside[] = {{0, 0}, {static_cast<short>(pm_width - 1), 0},
            {static_cast<short>(pm_width - 1), static_cast<short>(pm_height - 1)},
            {0, static_cast<short>(pm_height - 1)}, {0, 0}};
        XSetForeground(display, gc, black_pixel);
        XDrawLines(display, *field_p, gc, field_outside, 5, CoordModeOrigin);

        // フィールドの壁など
        for (const auto& ld : field_lines) {
            drawFieldLine_impl(ld, black_pixel);
        }
        for (const auto& ad : field_arcs) {
            drawFieldArc_impl(ad, black_pixel);
        }
        if (pos_history.history.size() >= 1) {
            Pos last_c = pos_history.history[0];
            for (auto& p : pos_history.history) {
                drawFieldLine_impl(p, last_c, orange_pixel);
                last_c = p;
            }
        }
        if (locus_history.history.size() >= 1) {
            Pos last_c = locus_history.history[0];
            for (auto& p : locus_history.history) {
                drawFieldLine_impl(p, last_c, blue_pixel);
                last_c = p;
            }
        }
    }
}

void ViewMap::drawFieldLine_impl(double x1, double y1, double x2, double y2, unsigned long pixel)
{
    if (v_display) {
        Display* display = static_cast<Display*>(*v_display);
        GC gc = static_cast<GC>(v_gc);
        XSetForeground(display, gc, pixel);
        XDrawLine(display, *field_p, gc, yFieldToWindow(y1), xFieldToWindow(x1), yFieldToWindow(y2),
            xFieldToWindow(x2));
    }
}

void ViewMap::drawWinLine_impl(double x1, double y1, double x2, double y2, unsigned long pixel)
{
    if (v_display) {
        Display* display = static_cast<Display*>(*v_display);
        GC gc = static_cast<GC>(v_gc);
        XSetForeground(display, gc, pixel);
        XDrawLine(display, win, gc, -field_ofs_x + yFieldToWindow(y1),
            -field_ofs_y + xFieldToWindow(x1), -field_ofs_x + yFieldToWindow(y2),
            -field_ofs_y + xFieldToWindow(x2));
    }
}

void ViewMap::drawFieldArc_impl(
    double x, double y, double r, double a1, double a2, unsigned long pixel)
{
    if (v_display) {
        Display* display = static_cast<Display*>(*v_display);
        GC gc = static_cast<GC>(v_gc);
        XSetForeground(display, gc, pixel);
        XDrawArc(display, *field_p, gc, yFieldToWindow(y + r), xFieldToWindow(x + r),
            static_cast<int>(round(r * 2 * zoom)), static_cast<int>(round(r * 2 * zoom)),
            static_cast<int>(round((a1 + 90) * 64)), static_cast<int>(round((a2 - a1) * 64)));
    }
}

}  // namespace XViewMap
