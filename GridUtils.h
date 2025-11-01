#pragma once
#include <Siv3D.hpp>
#include "Board.h"
#include "Utils.h"
#include "Config.h"

// ===================== ユーティリティ（マップロジック） =====================
inline s3d::Array<s3d::Point> LineCells(const s3d::Point& a, const s3d::Point& b) {
	using namespace s3d;
	Array<Point> out;
	int x0 = a.x, y0 = a.y, x1 = b.x, y1 = b.y;
	const int dx = std::abs(x1 - x0), sx = (x0 < x1 ? 1 : -1);
	const int dy = -std::abs(y1 - y0), sy = (y0 < y1 ? 1 : -1);
	int err = dx + dy;
	while (true) {
		out << Point{ x0, y0 };
		if (x0 == x1 && y0 == y1) break;
		int e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
	return out;
}

inline s3d::Point RaycastUntilWall(const Board& brd, const s3d::Point& a, const s3d::Point& b) {
	using namespace s3d;
	const auto path = LineCells(a, b);
	Point last = a;
	for (int i = 0; i < (int)path.size(); ++i) {
		const Point c = path[i];
		if (!brd.inBounds(c.x, c.y)) return last;
		const TileKind k = brd.tiles[brd.idx(c.x, c.y)].kind;
		if (k == TileKind::Wall) return last;
		last = c;
	}
	return last;
}

inline double TileDist(const s3d::Point& a, const s3d::Point& b) {
	const double dx = (double)(a.x - b.x);
	const double dy = (double)(a.y - b.y);
	return s3d::Math::Sqrt(dx * dx + dy * dy);
}

inline std::pair<double, double> Ownership(const Board& brd) {
	int blue = 0, red = 0;
	const int total = GW * GH;
	for (const auto& t : brd.tiles) {
		if (t.paint > 0.60f) ++blue;
		else if (t.paint < 0.40f) ++red;
	}
	return { (double)blue / total, (double)red / total };
}
