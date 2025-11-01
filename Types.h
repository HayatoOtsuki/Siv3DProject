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


// 元の描画スイッチを関数化（rc と base 色を渡して描く）
void DrawStructureGlyph(StructureType type, const RectF& rc, const ColorF& base) {
	switch (type) {
	case StructureType::Basic:
		rc.draw(base.withAlpha(0.85));
		rc.drawFrame(2, ColorF{ 0,0,0,0.65 });
		break;

	case StructureType::Sprinkler:
		Circle{ rc.center(), rc.w * 0.32 }.draw(base);
		Circle{ rc.center(), rc.w * 0.50 }.drawFrame(2, base);
		break;

	case StructureType::Pump:
		Triangle{ rc.center(), rc.w * 0.9, 0_deg }.draw(base);
		break;

	case StructureType::Sniper:
		RectF{ Arg::center = rc.center(), rc.w * 0.9, rc.h * 0.55 }.draw(base);
		break;

	case StructureType::Mortar:
		Circle{ rc.center(), rc.w * 0.42 }.draw(base);
		RectF{ Arg::center = rc.center().movedBy(0, -rc.h * 0.15), rc.w * 0.28, rc.h * 0.35 }.draw(ColorF{ 0 });
		break;

	case StructureType::HQ:
		Circle{ rc.center(), rc.w * 0.60 }.drawFrame(3, base);
		break;

	case StructureType::spawner:
		Circle{ rc.center(), rc.w * 0.36 }.draw(base);
		for (int i = 0; i < 6; ++i) {
			const double ang = i * (Math::TwoPi / 6.0);
			Vec2 p = rc.center() + Vec2{ Cos(ang), Sin(ang) } *rc.w * 0.55;
			Circle{ p, rc.w * 0.10 }.draw(base);
		}
		rc.drawFrame(2, ColorF{ 0,0,0,0.65 });
		break;
	}
}

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
// 指定サイズ・色でアイコン画像を作る（透過PNG）
Image MakeStructureIcon(StructureType type, const Size& size, const ColorF& base, double padding = 8.0) {
	RenderTexture rt{ size, ColorF{ 0,0,0,0 } };
	{
		const ScopedRenderTarget2D _rt{ rt };
		rt.clear(ColorF{ 0,0,0,0 });
		const RectF rc{ padding, padding, size.x - padding * 2.0, size.y - padding * 2.0 };
		DrawStructureGlyph(type, rc, base);
	}
	Graphics2D::Flush();               // GPU 完了を待つ
	Image img{ size };                 // 受け皿を用意
	rt.readAsImage(img);               // ← ここがポイント（void 戻り）
	return img;
}

	String FileNameFor(StructureType t) {
		switch (t) {
		case StructureType::Basic:     return U"rom/normal_turret.png";
		case StructureType::Sprinkler: return U"rom/sprinkler.png";
		case StructureType::Pump:      return U"rom/pump.png";
		case StructureType::Sniper:    return U"rom/sniper.png";
		case StructureType::Mortar:    return U"rom/canon.png";
		case StructureType::HQ:        return U"rom/shield_after.png";
		case StructureType::spawner:   return U"rom/spawner.png";
		default:                       return U"rom/normal_turret.png";
		}
	}
