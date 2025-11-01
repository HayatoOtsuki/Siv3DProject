#pragma once
#include <Siv3D.hpp>
#include "Config.h"

// ===================== 基本 =====================
enum class Team : int32 { None = 0, Blue = 1, Red = 2 };
inline s3d::ColorF TeamColor(Team t) {
	using namespace s3d;
	if (t == Team::Blue) return ColorF{ 0.20, 0.55, 1.0 };
	if (t == Team::Red)  return ColorF{ 1.00, 0.35, 0.35 };
	return ColorF{ 0.85 };
}
enum class Phase : int32 { Planning, Simulating, Summary };
enum class TileKind : int32 { Floor = 0, Wall = 1, HQBlue = 2, HQRed = 3 };

enum class StructureType : int32 {
	Basic = 0,
	Sprinkler = 1,
	Pump = 2,
	Sniper = 3,
	Mortar = 4,
	HQ = 5,
	spawner = 6,
};

// タレット仕様（射程：タイル、発射回数、弾仕様）
struct TypeSpec {
	int   cost = 0;
	double maxHP = 0;
	int   range = 0;           // タイル距離
	int   shots = 0;           // 自動フェーズ10秒での発射回数
	double damage = 0;         // 構造物へのダメージ
	double paint = 0;          // タイル塗り変化量（Blue:+ / Red:-）
	int   aoeRadius = 0;       // AoE半径（タイル） 0=単体
	bool  blockedByWalls = true; // 壁で遮蔽されるか
	bool  targetTurretOnly = false; // タレット（またはHQ）狙い
	bool  indirect = false;    // 壁を無視（迫撃砲）
	double spread = 0;         // 着弾ブレ（タイル）
	double projSpeed = 800.0;  // 弾速度（px/s）
	double projRadius = 5.0;   // 見た目用の弾半径（px）
};

inline const TypeSpec& GetSpec(StructureType t) {
	static TypeSpec sBasic{
		CostBasic, HPBasic,
		6, 10, 18.0, 0.18, 0, true, false, false, 1.5, 780.0, 5.0
	};
	static TypeSpec sSprinkler{
		CostSprinkler, HPSprinkler,
		3, 22, 0.0, 0.12, 0, false, false, false, 0.0, 520.0, 4.0
	};
	static TypeSpec sPump{
		CostPump, HPPump,
		0, 0, 0.0, 0.0, 0, false, false, false, 0.0, 0.0, 0.0
	};
	static TypeSpec sSniper{
		CostSniper, HPSniper,
		12, 2, 120.0, 0.02, 0, true, true, false, 0.0, 1400.0, 4.0
	};
	static TypeSpec sMortar{
		CostMortar, HPMortar,
		10, 3, 35.0, 0.10, 2, false, false, true, 2.0, 540.0, 6.0
	};
	static TypeSpec sHQ{
		0, HPHQ, 0, 0, 0, 0, 0, false, false, false, 0.0, 0.0, 0.0
	};
	static TypeSpec sSpawner{
		CostSpawner, HPSpawner,
		0, 0, 0, 0, 0, false, false, false, 0.0, 0.0, 0.0
	};

	switch (t) {
	case StructureType::Basic:      return sBasic;
	case StructureType::Sprinkler:  return sSprinkler;
	case StructureType::Pump:       return sPump;
	case StructureType::Sniper:     return sSniper;
	case StructureType::Mortar:     return sMortar;
	case StructureType::HQ:         return sHQ;
	case StructureType::spawner:    return sSpawner;
	default:                        return sBasic;
	}
}
