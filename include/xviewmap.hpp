#pragma once
#include "position.hpp"
#include <array>
#include <optional>
#include <thread>
#include <vector>
#include <utility>

namespace XViewMap
{
class ViewMap
{
public:
    ViewMap();
    ~ViewMap();

    // ロボットの位置を更新
    void updatePos(const Pos& pos, const Pos& vel);
    void updatePos(double x, double y, double th, double vx, double vy, double omg)
    {
        updatePos({x, y, th}, {vx, vy, omg});
    }
    // 軌跡を描画せず位置を移動
    void resetPos(const Pos& pos);
    void resetPos(double x, double y, double th) { resetPos({x, y, th}); }

    void updateLocus(const Pos& pos);
    void updateLocus(double x, double y, double th) { updateLocus({x, y, th}); }
    void resetLocus(const Pos& pos);
    void resetLocus(double x, double y, double th) { resetLocus({x, y, th}); }

    // フィールドサイズを設定
    void setField(double min_x, double min_y, double max_x, double max_y);
    // フィールドに線を引く(壁とか)
    void drawFieldLine(double x1, double y1, double x2, double y2);
    // フィールドに円or円弧を描く
    // x,yが原点、角度a1〜a2の範囲の円弧を描く(度、0はxの方向、a1=0 a2=360で円)
    void drawFieldArc(double x, double y, double r, double a1 = 0, double a2 = 360);
    // 駆動輪の位置と角度(描画用、個数は任意)
    std::vector<Pos> wheels = {
        {200, 200, 135 * 3.14 / 180},
        {-200, 200, -135 * 3.14 / 180},
        {-200, -200, -45 * 3.14 / 180},
        {200, -200, 45 * 3.14 / 180},
    };
    double wheel_radius = 50;
    // マシンの形状(適当)
    std::vector<Pos> machine = {
        {150, 150},
        {-150, 150},
        {-150, -150},
        {150, -150},
        {200, 0},
    };

    // 指定したtomlファイルを読み込む
    void readToml(const std::string& path);
    // xviewmap.toml を読み込む
    void readToml();

private:
    std::mutex x11_mutex;
    std::optional<std::thread> win_thread, pos_thread, locus_thread;
    void winThread();
    void posThread(PositionHistory& history, unsigned long pixel);

    // X11/Xlib.hをincludeするとdefine祭りで治安最悪になるので他の型で代用
    std::optional<void* /* Display* */> v_display;
    unsigned long /* Window */ win;
    void* /* GC aka _XGC* */ v_gc;
    unsigned long black_pixel, white_pixel, red_pixel, orange_pixel, forestgreen_pixel, blue_pixel;
    std::optional<unsigned long /*Pixmap*/> field_p;
    int screen_num;
    void flush();

    // 画面座標系: 左上原点、右がx、下がy
    int win_width, win_height;             // 画面の幅、高さ
    int field_ofs_x = 0, field_ofs_y = 0;  // フィールドの位置(画面座標系)
    // フィールド座標系: 上がx, 左がy
    double field_min_x, field_min_y, field_max_x, field_max_y;
    double field_width, field_height;
    double zoom;  // 画面座標=フィールド座標*zoom

    void resetFieldZoom();
    // フィールド座標→画面座標の変換
    int xFieldToWindow(double x);
    int yFieldToWindow(double y);

    void resetPixmap();
    void updateWindow();

    using LineData = std::pair<Pos, Pos>;
    struct ArcData {
        double x, y, r, a1, a2;
    };
    std::vector<LineData> field_lines = {};
    std::vector<ArcData> field_arcs = {};
    void drawFieldLine_impl(double x1, double y1, double x2, double y2, unsigned long pixel);
    void drawFieldLine_impl(const Pos& p1, const Pos& p2, unsigned long pixel)
    {
        drawFieldLine_impl(p1.x, p1.y, p2.x, p2.y, pixel);
    }
    void drawFieldLine_impl(const LineData& ld, unsigned long pixel)
    {
        drawFieldLine_impl(ld.first, ld.second, pixel);
    }
    void drawWinLine_impl(double x1, double y1, double x2, double y2, unsigned long pixel);
    void drawWinLine_impl(const Pos& p1, const Pos& p2, unsigned long pixel)
    {
        drawWinLine_impl(p1.x, p1.y, p2.x, p2.y, pixel);
    }
    void drawWinLine_impl(const LineData& ld, unsigned long pixel)
    {
        drawWinLine_impl(ld.first, ld.second, pixel);
    }
    void drawFieldArc_impl(double x, double y, double r, double a1, double a2, unsigned long pixel);
    void drawFieldArc_impl(const ArcData& ad, unsigned long pixel)
    {
        drawFieldArc_impl(ad.x, ad.y, ad.r, ad.a1, ad.a2, pixel);
    }
    double vel_x, vel_y;
    PositionHistory pos_history{}, locus_history{};
};

}  // namespace XViewMap
