#pragma once
#include "window.h"

void tileMasterStack(std::vector<Window> &wins, int sw, int sh, const Config &cfg);
void tileBSP(std::vector<Window> &wins, int sw, int sh, const Config &cfg);
void tileGrid(std::vector<Window> &wins, int sw, int sh, const Config &cfg);
