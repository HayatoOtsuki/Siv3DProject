#pragma once
#include <Siv3D.hpp>
#include "Entities.h"
#include "Config.h"

struct Board {
	s3d::Array<Tile> tiles;      // GW*GH
	s3d::Array<int> blueIndex;   // 各セルの味方構造物インデックス（-1=なし）
	s3d::Array<int> redIndex;    // 各セルの敵構造物インデックス（-1=なし）
	s3d::RectF gridRect;
	double tileSize = 32.0;

	void init() {
		tiles.assign(GW * GH, Tile{});
		blueIndex.assign(GW * GH, -1);
		redIndex.assign(GW * GH, -1);
	}

	inline int idx(int x, int y) const noexcept { return (y * GW + x); }
	bool inBounds(int x, int y) const noexcept {
		return (0 <= x && x < GW && 0 <= y && y < GH);
	}

	s3d::Optional<s3d::Point> screenToCell(const s3d::Vec2& sp) const {
		if (!gridRect.intersects(sp)) return s3d::none;
		const s3d::Vec2 q = sp - gridRect.pos;
		const int cx = static_cast<int>(q.x / tileSize);
		const int cy = static_cast<int>(q.y / tileSize);
		if (!inBounds(cx, cy)) return s3d::none;
		return s3d::Point{ cx, cy };
	}

	// 画面座標->セル（盤面外は none）
	s3d::Optional<s3d::Point> posToCell(const s3d::Vec2& p) const {
		return screenToCell(p);
	}

	s3d::Vec2 cellCenter(const s3d::Point& c) const {
		return gridRect.pos + s3d::Vec2{ (c.x + 0.5) * tileSize, (c.y + 0.5) * tileSize };
	}
	s3d::RectF cellRect(const s3d::Point& c) const {
		return s3d::RectF{ gridRect.pos + s3d::Vec2{ c.x * tileSize, c.y * tileSize }, tileSize, tileSize };
	}
};
