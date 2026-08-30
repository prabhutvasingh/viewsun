#include "../include/tiling.h"
#include <cmath>

void tileMasterStack(std::vector<Window> &wins, int sw, int sh, const Config &cfg) {
    int n = wins.size();
    if (n == 0) return;
    int gap = cfg.gap;
    int masterW = (n > 1) ? sw * cfg.master_ratio / 100 : sw;
    int stackW = sw - masterW;

    int nmaster = std::min(n, cfg.master_count);
    int nstack = n - nmaster;

    // smart master area: fill without empty - vertical for 3, grid only 4+
    if (nmaster > 0) {
        if (nmaster <= 3) {
            int availH_master = sh - gap * (nmaster + 1);
            int h_master = availH_master / nmaster;
            for (int i = 0; i < nmaster; i++) {
                wins[i].rect.x = gap;
                wins[i].rect.y = gap + i * (h_master + gap);
                wins[i].rect.w = masterW - 2*gap + (n==1 ? gap : 0);
                if (n==1) wins[i].rect.w = sw - 2*gap;
                wins[i].rect.h = h_master;
                if (i == nmaster-1) wins[i].rect.h += availH_master % nmaster;
            }
        } else {
            // smart grid for master: fill without empty - stretch last row
            int cols = (int)ceil(sqrt(nmaster));
            int rows = (int)ceil((double)nmaster / cols);
            int cellH = (sh - gap*(rows+1)) / rows;
            for (int i=0;i<nmaster;i++) {
                int row=i/cols, col=i%cols;
                int colsInRow = std::min(cols, nmaster - row*cols);
                int cellWrow = (masterW - gap*(colsInRow+1)) / colsInRow;
                wins[i].rect.x = gap + col*(cellWrow+gap);
                wins[i].rect.y = gap + row*(cellH+gap);
                wins[i].rect.w = cellWrow;
                wins[i].rect.h = cellH;
            }
        }
    }
    // smart stack area: both directions - fill without empty (<=3 vertical, 4+ grid)
    if (nstack > 0) {
        if (nstack <= 3) {
            int availH_stack = sh - gap * (nstack + 1);
            int h_stack = availH_stack / nstack;
            for (int i = 0; i < nstack; i++) {
                int idx = nmaster + i;
                wins[idx].rect.x = masterW + gap;
                wins[idx].rect.y = gap + i * (h_stack + gap);
                wins[idx].rect.w = stackW - 2*gap;
                wins[idx].rect.h = h_stack;
                if (i == nstack-1) wins[idx].rect.h += availH_stack % nstack;
            }
        } else {
            int cols = (int)ceil(sqrt(nstack));
            int rows = (int)ceil((double)nstack / cols);
            int cellH = (sh - gap*(rows+1)) / rows;
            for (int i=0;i<nstack;i++) {
                int idx = nmaster + i;
                int row=i/cols, col=i%cols;
                int colsInRow = std::min(cols, nstack - row*cols);
                int cellWrow = (stackW - gap*(colsInRow+1)) / colsInRow;
                wins[idx].rect.x = masterW + gap + col*(cellWrow+gap);
                wins[idx].rect.y = gap + row*(cellH+gap);
                wins[idx].rect.w = cellWrow;
                wins[idx].rect.h = cellH;
            }
        }
    }
}

static void bspRecurse(std::vector<Window> &wins, int start, int end, Rect area, bool horiz, int gap) {
    int n = end - start;
    if (n <= 0) return;
    if (n == 1) {
        wins[start].rect = {area.x + gap, area.y + gap, area.w - 2*gap, area.h - 2*gap};
        return;
    }
    int mid = start + n/2;
    if (horiz) {
        int w1 = area.w / 2;
        Rect a1{area.x, area.y, w1, area.h};
        Rect a2{area.x + w1, area.y, area.w - w1, area.h};
        bspRecurse(wins, start, mid, a1, !horiz, gap);
        bspRecurse(wins, mid, end, a2, !horiz, gap);
    } else {
        int h1 = area.h / 2;
        Rect a1{area.x, area.y, area.w, h1};
        Rect a2{area.x, area.y + h1, area.w, area.h - h1};
        bspRecurse(wins, start, mid, a1, !horiz, gap);
        bspRecurse(wins, mid, end, a2, !horiz, gap);
    }
}

void tileBSP(std::vector<Window> &wins, int sw, int sh, const Config &cfg) {
    if (wins.empty()) return;
    Rect area{0,0,sw,sh};
    bspRecurse(wins, 0, wins.size(), area, true, cfg.gap/2);
    // adjust for gap already applied inside recurse, fix outer
    for (auto &w: wins) {
        // ensure inside screen
        if (w.rect.x < 0) w.rect.x = 0;
        if (w.rect.y < 0) w.rect.y = 0;
    }
}

void tileGrid(std::vector<Window> &wins, int sw, int sh, const Config &cfg) {
    int n = wins.size();
    if (n==0) return;
    int cols = (int)ceil(sqrt(n));
    int rows = (int)ceil((double)n / cols);
    int gap = cfg.gap;
    int cellH = (sh - gap*(rows+1)) / rows;
    for (int i=0;i<n;i++) {
        int row=i/cols, col=i%cols;
        int colsInRow = std::min(cols, n - row*cols);
        int cellWrow = (sw - gap*(colsInRow+1)) / colsInRow;
        wins[i].rect.x = gap + col*(cellWrow+gap);
        wins[i].rect.y = gap + row*(cellH+gap);
        wins[i].rect.w = cellWrow;
        wins[i].rect.h = cellH;
    }
}
