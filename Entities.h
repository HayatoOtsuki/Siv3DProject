#pragma once
#include <Siv3D.hpp>
#include "Types.h"
#include "Config.h"

// ===================== データ構造 =====================
struct Tile {
	TileKind kind = TileKind::Floor;
	float paint = 0.5f; // 0..1（0=Red、1=Blue）
};

struct Structure {
	Team owner = Team::Blue;
	StructureType type = StructureType::Basic;
	s3d::Point cell{ 0, 0 };
	double hp = 1.0;
	bool alive = true;
	double nextFire = 0.0;
	double interval = 1.0;
};

struct Tracer {
	s3d::Vec2 p0, p1;
	s3d::ColorF col;
	double age = 0.0, life = 0.18;
};

struct Particle {
	s3d::Vec2 pos, vel;
	s3d::ColorF col;
	double age = 0.0, life = 0.5;
	double size0 = 3.0, size1 = 12.0;
};

// 実弾（見える弾）
enum class ProjKind : int32 { Bullet, Droplet, Sniper, Mortar };
struct Projectile {
	ProjKind kind = ProjKind::Bullet;
	Team owner = Team::Blue;
	s3d::Vec2 pos;
	s3d::Vec2 vel;            // 直進用
	double radius = 5.0;      // 見た目
	// 目的
	s3d::Point targetCell{ -1, -1 };
	// 当たり判定/効果
	bool blockedByWalls = true;
	bool indirect = false;
	int aoeRadius = 0;     // 0=単体
	double damage = 0.0;
	double paint = 0.0;
	// 寿命・管理
	double age = 0.0, life = 3.0;
	// 放物線用
	bool useArc = false;
	s3d::Vec2 startPos, endPos, apexPos;
	double u = 0.0;        // 経路の進捗0..1
	double pathLen = 1.0;  // 経路長（px）
	double pathSpeed = 600.0; // 経路に沿って進む速度（px/s）
};

// 歩行ユニット（プレイヤー / 敵）
struct Actor {
	s3d::Vec2 pos{ 0,0 };
	s3d::Vec2 lastPos{ 0,0 };
	double radius = 10.0;
	double speed = 200.0;
	double hp = 100.0;
	bool alive = false;
	// クリック移動用
	s3d::Optional<s3d::Vec2> moveTarget;
	// 時間で消える
	double life = 0.0; // 秒
	double age = 0.0;  // 経過秒
};
