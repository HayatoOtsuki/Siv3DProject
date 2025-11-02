#pragma once
#include <Siv3D.hpp>
#include "Config.h"
#include "Utils.h"
#include "Types.h"
#include "Entities.h"
#include "Board.h"
#include "GridUtils.h"
#include "audio.h"


class Game {
public:
	Board brd;
	Phase phase = Phase::Planning;

	// ステージ
	int stage = 1;
	bool stageStarting = true;
	double stageBannerT = 0.0; // 0->1でアニメ
	bool stageCleared = false;

	// 経済
	int moneyBlue = 200;
	int moneyRed = 1040;
	int turnCount = 1;

	// プレイヤーの選択
	StructureType selectedType = StructureType::Basic;

	// 構造物
	s3d::Array<Structure> blues;
	s3d::Array<Structure> reds;

	// HQインデックス
	int blueHQ = -1;
	int redHQ = -1;

	// シミュレーション
	double simTime = 0.0;
	double simElapsed = 0.0;

	// プレイヤー
	s3d::Optional<Actor> player;

	// 敵ユニット（AI）
	s3d::Array<Actor> redAgents;

	// 視覚演出
	s3d::Array<Tracer> tracers;
	s3d::Array<Particle> particles;
	s3d::Array<Projectile> projectiles;

	// 画面振動・ヒットストップ
	double shakeT = 0.0, shakeDur = 0.0, shakePow = 0.0;
	double timeScale = 1.0, hitStopTimer = 0.0;

public:
	// レイアウト
	void layout();

	// マップ生成
	void buildMapForStage(int stageNo);

	// 画面シェイク・ヒットストップ
	void AddShake(double p, double d);
	s3d::Vec2 GetShakeOffset();
	void updateEffectsEveryFrame(double dtReal);
	void clearShakeAndHitStop();

	// パーティクル
	void SpawnParticles(const s3d::Vec2& p, const s3d::ColorF& col, int n, double vmin = 80, double vmax = 180, double lifeMin = 0.25, double lifeMax = 0.6, double s0 = 3, double s1 = 14);

	// 勝敗
	bool isBlueWin() const;
	bool isBlueLose() const;

	// フェーズ
	void beginSimulation();
	void endSimulationAndScore();
	void gotoNextStage();

	// シミュレーション更新
	void updateSimulation(double dtReal);
	// プランニング更新
	void updatePlanning();
	// サマリー更新
	void updateSummary();

	// 描画
	void drawBoard() const;
	void drawStructures() const;
	void drawTracers() const;
	void drawParticles() const;
	void drawProjectiles() const;
	void drawPlayer() const;
	void drawEnemies() const;
	void drawUI();
	void drawHoverHelp() const;
	void drawStageBanner() const;

private:
	// 置けるか判定・設置
	bool canPlace(Team side, StructureType type, const s3d::Point& c, s3d::String& reason) const;
	bool placeBlue(StructureType type, const s3d::Point& c);

	// 敵AI 設置
	void enemyPlaceAI();

	// 塗り・ダメージ
	void applyPaintAt(const s3d::Point& c, double delta);
	void damageAt(const s3d::Point& c, double dmg, Team attacker);
	void applyAOE(const s3d::Point& center, int r, double paintDelta, double dmg, Team atk);

	// 射撃系
	s3d::Optional<s3d::Point> findTargetCell(Team atk, const s3d::Point& from, int range, bool turretOnly) const;
	void spawnProjectile(Team atk, const TypeSpec& spec, const s3d::Vec2& muzzle, const s3d::Point& targetCell, ProjKind k, bool useArc, bool blocked, bool indirect, int aoe, double dmg, double paint, double speed, double radiusPx);
	void fireOnce(Structure& s, Team atk);
	void updateProjectiles(double dtReal);
	void impactAt(const Projectile& pr, const s3d::Point& ic);

	// プレイヤー・敵ユニット
	bool isWallAt(const s3d::Vec2& p) const;
	void moveWithCollide(Actor& a, const s3d::Vec2& delta);

	bool trySpawnFromClickedSpawner();
	void spawnPlayerAt(const s3d::Point& c);
	void playerExplodeAt(const s3d::Point& cell);
	void paintTrailByMove(const s3d::Vec2& from, const s3d::Vec2& to, double dt);
	void updatePlayer(double dt);

	void spawnEnemyAt(const s3d::Point& c);
	void enemyExplodeAt(const s3d::Point& cell);
	s3d::Vec2 enemySeekTargetVec(const Actor& e) const;
	void updateEnemySpawnerProduction(double dt);
	void updateRedAgents(double dt);

};
