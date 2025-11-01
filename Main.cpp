# include <Siv3D.hpp> // Siv3D v0.6.16

// ===================== ユーティリティ（Clamp 使わない） =====================
template <class T>
inline T limit(const T& x, const T& lo, const T& hi) {
	return (x < lo ? lo : (x > hi ? hi : x));
}
inline double saturate(double x) { return limit(x, 0.0, 1.0); }

// 簡易イージング（バナー表示など）
inline double easeOutBack(double t) { t = saturate(t); const double c1 = 1.70158, c3 = c1 + 1.0; return 1 + c3 * Pow(t - 1, 3) + c1 * Pow(t - 1, 2); }
inline double easeInOutSine(double t) { t = saturate(t); return -0.5 * (Cos(Math::Pi * t) - 1.0); }

// ===================== 基本 =====================
enum class Team : int32 { None = 0, Blue = 1, Red = 2 };
inline ColorF TeamColor(Team t) {
	if (t == Team::Blue) return ColorF{ 0.20, 0.55, 1.0 };
	if (t == Team::Red)  return ColorF{ 1.00, 0.35, 0.35 };
	return ColorF{ 0.85 };
}
enum class Phase : int32 { Planning, Simulating, Summary };
enum class TileKind : int32 { Floor = 0, Wall = 1, HQBlue = 2, HQRed = 3 };

enum class StructureType : int32 {
	Basic = 0,        // 基本タレット：射程・弾数ふつう、ブレる
	Sprinkler = 1,    // スプリンクラー：短射程・多弾（小弾が飛ぶ）
	Pump = 2,         // インクポンプ：収益 +α
	Sniper = 3,       // スナイパー：極長射程・貫通不可、敵タレット狙撃
	Mortar = 4,       // 迫撃砲：高射程・壁越え爆撃（放物線・範囲）
	HQ = 5,           // 本拠地（内部用）
	spawner = 6,      // スポナー（クリックでプレイヤー出撃 / 敵はAIで出撃）
};

// ===================== マップ設定 =====================
static constexpr int GW = 36;
static constexpr int GH = 20;

// 画面レイアウト
static constexpr double UIWidth = 360.0;
static constexpr double Margin = 12.0;

// ターン/シミュレーション
static constexpr double SimDuration = 10.0;

// 経済
static constexpr int CostBasic = 40;
static constexpr int CostSprinkler = 35;
static constexpr int CostPump = 60;
static constexpr int CostSniper = 90;
static constexpr int CostMortar = 80;
// スポナーのコスト
static constexpr int CostSpawner = 100;
static constexpr int IncomePerTile = 1;
static constexpr int IncomePerPump = 20;

// 構造物 HP
static constexpr double HPBasic = 120.0;
static constexpr double HPSprinkler = 90.0;
static constexpr double HPPump = 120.0;
static constexpr double HPSniper = 80.0;
static constexpr double HPMortar = 100.0;
static constexpr double HPHQ = 600.0;
static constexpr double HPSpawner = 140.0;

// プレイヤー（出撃ユニット：クリックでスポナーから出現）
static constexpr int HPPlayer = 200;
static constexpr double PlayerSpeed = 220.0;
static constexpr double PlayerRadius = 10.0;
static constexpr double PlayerPaintPerSecond = 0.35; // 移動で塗る量（毎秒）
// 体当たり爆発（プレイヤー）
static constexpr int PlayerExplodeRadius = 2;           // 爆発半径（タイル）
static constexpr double PlayerExplodeDamage = 50.0;    // 爆発ダメージ
static constexpr double PlayerExplodePaint = 0.60;      // 爆発で塗る量（中心最大）
static constexpr double PlayerExplodeShakePow = 16.0;   // 画面揺れ（大きめ）
static constexpr double PlayerExplodeShakeDur = 0.22;   // 揺れ時間
static constexpr double PlayerExplodeHitstop = 0.06;    // ヒットストップ

// 敵ユニット（AI出撃・体当たり）
static constexpr int HPEnemy = 200;
static constexpr double EnemySpeed = 180.0;
static constexpr double EnemyRadius = 10.0;
// 体当たり爆発（敵）
static constexpr int EnemyExplodeRadius = 2;
static constexpr double EnemyExplodeDamage = 50.0;
static constexpr double EnemyExplodePaint = 0.55;
static constexpr double EnemyExplodeShakePow = 12.0;
static constexpr double EnemyExplodeShakeDur = 0.18;
static constexpr double EnemyExplodeHitstop = 0.04;

// 敵スポナーの出撃間隔（シミュレーション中）
static constexpr double EnemySpawnerInterval = 4.0;

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

// ===================== データ構造 =====================
struct Tile {
	TileKind kind = TileKind::Floor;
	float paint = 0.5f; // 0..1（0=Red、1=Blue）
};

struct Structure {
	Team owner = Team::Blue;
	StructureType type = StructureType::Basic;
	Point cell{ 0, 0 };
	double hp = 1.0;
	bool alive = true;
	double nextFire = 0.0;
	double interval = 1.0;
};

struct Tracer {
	Vec2 p0, p1;
	ColorF col;
	double age = 0.0, life = 0.18;
};

struct Particle {
	Vec2 pos, vel;
	ColorF col;
	double age = 0.0, life = 0.5;
	double size0 = 3.0, size1 = 12.0;
};

// 実弾（見える弾）
enum class ProjKind : int32 { Bullet, Droplet, Sniper, Mortar };
struct Projectile {
	ProjKind kind = ProjKind::Bullet;
	Team owner = Team::Blue;
	Vec2 pos;
	Vec2 vel;            // 直進用
	double radius = 5.0; // 見た目
	// 目的
	Point targetCell{ -1, -1 };
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
	Vec2 startPos, endPos, apexPos;
	double u = 0.0;        // 経路の進捗0..1
	double pathLen = 1.0;  // 経路長（px）
	double pathSpeed = 600.0; // 経路に沿って進む速度（px/s）
};

// 歩行ユニット（プレイヤー / 敵）
struct Actor {
	Vec2 pos{ 0,0 };
	Vec2 lastPos{ 0,0 };
	double radius = 10.0;
	double speed = 200.0;
	double hp = 100.0;
	bool alive = false;
};

// ===================== マップと描画座標 =====================
struct Board {
	Array<Tile> tiles;      // GW*GH
	Array<int> blueIndex;   // 各セルの味方構造物インデックス（-1=なし）
	Array<int> redIndex;    // 各セルの敵構造物インデックス（-1=なし）
	RectF gridRect;
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

	Optional<Point> screenToCell(const Vec2& sp) const {
		if (!gridRect.intersects(sp)) return none;
		const Vec2 q = sp - gridRect.pos;
		const int cx = static_cast<int>(q.x / tileSize);
		const int cy = static_cast<int>(q.y / tileSize);
		if (!inBounds(cx, cy)) return none;
		return Point{ cx, cy };
	}

	// 画面座標->セル（盤面外は none）
	Optional<Point> posToCell(const Vec2& p) const {
		return screenToCell(p);
	}

	Vec2 cellCenter(const Point& c) const {
		return gridRect.pos + Vec2{ (c.x + 0.5) * tileSize, (c.y + 0.5) * tileSize };
	}
	RectF cellRect(const Point& c) const {
		return RectF{ gridRect.pos + Vec2{ c.x * tileSize, c.y * tileSize }, tileSize, tileSize };
	}
};

// ===================== ユーティリティ（マップロジック） =====================
static Array<Point> LineCells(const Point& a, const Point& b) {
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

static Point RaycastUntilWall(const Board& brd, const Point& a, const Point& b) {
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

static double TileDist(const Point& a, const Point& b) {
	const double dx = (double)(a.x - b.x);
	const double dy = (double)(a.y - b.y);
	return Math::Sqrt(dx * dx + dy * dy);
}

static std::pair<double, double> Ownership(const Board& brd) {
	int blue = 0, red = 0;
	const int total = GW * GH;
	for (const auto& t : brd.tiles) {
		if (t.paint > 0.60f) ++blue;
		else if (t.paint < 0.40f) ++red;
	}
	return { (double)blue / total, (double)red / total };
}

// ===================== 構造体管理 =====================
struct Game {
	Board brd;
	Phase phase = Phase::Planning;

	// ステージ
	int stage = 1;
	bool stageStarting = true;
	double stageBannerT = 0.0; // 0->1でアニメ
	bool stageCleared = false;

	// 経済
	int moneyBlue = 140;
	int moneyRed = 140;
	int turnCount = 1;

	// プレイヤーの選択
	StructureType selectedType = StructureType::Basic;

	// 構造物
	Array<Structure> blues;
	Array<Structure> reds;

	// HQインデックス
	int blueHQ = -1;
	int redHQ = -1;

	// シミュレーション
	double simTime = 0.0;
	double simElapsed = 0.0;

	// プレイヤー
	Optional<Actor> player;

	// 敵ユニット（AI）
	Array<Actor> redAgents;

	// 視覚演出
	Array<Tracer> tracers;
	Array<Particle> particles;
	Array<Projectile> projectiles;

	// 画面振動・ヒットストップ
	double shakeT = 0.0, shakeDur = 0.0, shakePow = 0.0;
	double timeScale = 1.0, hitStopTimer = 0.0;

	// レイアウト
	void layout() {
		const double leftW = Scene::Width() - UIWidth - 2 * Margin;
		const double leftH = Scene::Height() - 2 * Margin;
		const double ts1 = leftW / GW;
		const double ts2 = leftH / GH;
		brd.tileSize = Min(ts1, ts2);
		const double gridW = brd.tileSize * GW;
		const double gridH = brd.tileSize * GH;
		brd.gridRect = RectF{ Margin, Margin, gridW, gridH };
	}

	// マップ生成（ステージごとに難易度を少し上げる）
	void buildMapForStage(int stageNo) {
		brd.init();

		for (int y = 0; y < GH; ++y) {
			for (int x = 0; x < GW; ++x) {
				Tile& t = brd.tiles[brd.idx(x, y)];
				bool makeWall = false;
				if ((x % 11 == 0) && (y % 5 != 0) && (x > 6 && x < GW - 6)) makeWall = true;
				if (stageNo >= 2 && (x % 13 == 6) && (y % 7 == 3)) makeWall = true;
				if (makeWall) {
					t.kind = TileKind::Wall;
					t.paint = 0.5f;
					continue;
				}
				if (x < GW / 2 - 2)      t.paint = 0.80f;
				else if (x > GW / 2 + 1) t.paint = 0.20f;
				else                     t.paint = 0.50f;
				t.kind = TileKind::Floor;
			}
		}

		// HQ 設置
		Point bHQ{ 3, GH / 2 };
		Point rHQ{ GW - 4, GH / 2 };
		brd.tiles[brd.idx(bHQ.x, bHQ.y)].kind = TileKind::HQBlue;
		brd.tiles[brd.idx(rHQ.x, rHQ.y)].kind = TileKind::HQRed;
		brd.tiles[brd.idx(bHQ.x, bHQ.y)].paint = 1.0f;
		brd.tiles[brd.idx(rHQ.x, rHQ.y)].paint = 0.0f;

		blues.clear(); reds.clear();
		tracers.clear(); particles.clear(); projectiles.clear();
		redAgents.clear();
		player.reset();
		stageCleared = false;
		turnCount = 1;
		phase = Phase::Planning;

		Structure sb; sb.owner = Team::Blue; sb.type = StructureType::HQ; sb.cell = bHQ; sb.hp = GetSpec(StructureType::HQ).maxHP; sb.alive = true;
		Structure sr; sr.owner = Team::Red;  sr.type = StructureType::HQ; sr.cell = rHQ; sr.hp = GetSpec(StructureType::HQ).maxHP; sr.alive = true;
		blues << sb; reds << sr;
		brd.blueIndex[brd.idx(bHQ.x, bHQ.y)] = 0;
		brd.redIndex[brd.idx(rHQ.x, rHQ.y)] = 0;
		blueHQ = 0; redHQ = 0;

		auto placeRed = [&](StructureType t, Point c) {
			if (!brd.inBounds(c.x, c.y)) return;
			if (brd.tiles[brd.idx(c.x, c.y)].kind != TileKind::Floor) return;
			if (brd.redIndex[brd.idx(c.x, c.y)] != -1) return;
			if (brd.blueIndex[brd.idx(c.x, c.y)] != -1) return;
			Structure s; s.owner = Team::Red; s.type = t; s.cell = c; s.hp = GetSpec(t).maxHP; s.alive = true;
			reds << s;
			brd.redIndex[brd.idx(c.x, c.y)] = (int)reds.size() - 1;
			brd.tiles[brd.idx(c.x, c.y)].paint = 0.2f;
			};
		placeRed(StructureType::Basic, Point{ GW - 7, GH / 2 - 3 });
		placeRed(StructureType::Sprinkler, Point{ GW - 8, GH / 2 + 2 });
		placeRed(StructureType::Mortar, Point{ GW - 10, GH / 2 });

		if (stageNo >= 2) {
			placeRed(StructureType::Basic, Point{ GW - 12, GH / 2 - 6 });
			placeRed(StructureType::Sniper, Point{ GW - 8,  GH / 2 - 1 });
		}
		if (stageNo >= 3) {
			placeRed(StructureType::Mortar, Point{ GW - 14, GH / 2 + 5 });
			placeRed(StructureType::Sprinkler, Point{ GW - 6,  GH / 2 + 6 });
		}

		moneyBlue = 120 + 20 * (stageNo - 1);
		moneyRed = 140 + 40 * (stageNo - 1);

		stageStarting = true;
		stageBannerT = 0.0;

		clearShakeAndHitStop();
	}

	// 画面シェイク
	void AddShake(double p, double d) { shakePow = Max(shakePow, p); shakeDur = Max(shakeDur, d); shakeT = Max(shakeT, d); }
	Vec2 GetShakeOffset() {
		if (shakeT <= 0.0 || shakeDur <= 0.0) return Vec2{ 0,0 };
		const double k = (shakeT / shakeDur);
		const double amp = shakePow * (0.25 + 0.75 * k);
		return RandomVec2(Circle{ amp });
	}

	// 演出更新（フェーズに関わらず毎フレーム呼ぶ）
	void updateEffectsEveryFrame(double dtReal) {
		if (hitStopTimer > 0.0) {
			hitStopTimer -= dtReal;
			if (hitStopTimer < 0.0) hitStopTimer = 0.0;
		}
		if (shakeT > 0.0) {
			shakeT -= dtReal;
			if (shakeT <= 0.0) { shakeT = 0.0; shakeDur = 0.0; shakePow = 0.0; }
		}
	}

	void clearShakeAndHitStop() {
		shakeT = shakeDur = shakePow = 0.0;
		hitStopTimer = 0.0;
		timeScale = 1.0;
	}

	// パーティクル
	void SpawnParticles(const Vec2& p, const ColorF& col, int n, double vmin = 80, double vmax = 180, double lifeMin = 0.25, double lifeMax = 0.6, double s0 = 3, double s1 = 14) {
		for (int i = 0; i < n; ++i) {
			const double a = Random(0.0, Math::TwoPi);
			const double sp = Random(vmin, vmax);
			Particle fx;
			fx.pos = p;
			fx.vel = Vec2{ Cos(a), Sin(a) } *sp;
			fx.col = col;
			fx.age = 0.0; fx.life = Random(lifeMin, lifeMax);
			fx.size0 = Random(s0 * 0.8, s0 * 1.2);
			fx.size1 = Random(s1 * 0.8, s1 * 1.2);
			particles << fx;
		}
	}

	// 勝敗条件チェック
	bool isBlueWin() const {
		auto [bp, rp] = Ownership(brd);
		const bool redHQDead = (redHQ >= 0 && !reds[redHQ].alive);
		return (bp >= 0.98) || redHQDead;
	}
	bool isBlueLose() const {
		auto [bp, rp] = Ownership(brd);
		const bool blueHQDead = (blueHQ >= 0 && !blues[blueHQ].alive);
		return (rp >= 0.98) || blueHQDead;
	}

	// 敵AIの簡易設置（スポナーも含める）
	void enemyPlaceAI() {
		int tries = 18;
		const int minCost = Min({ CostBasic, CostSprinkler, CostMortar, CostSpawner });
		while (tries-- > 0) {
			if (moneyRed < minCost) break;

			Array<StructureType> bag;
			if (moneyRed >= CostBasic)     bag << StructureType::Basic;
			if (moneyRed >= CostSprinkler) bag << StructureType::Sprinkler;
			if (moneyRed >= CostMortar)    bag << StructureType::Mortar;
			if (stage >= 2 && moneyRed >= CostSniper && RandomBool(0.35)) bag << StructureType::Sniper;
			if (moneyRed >= CostPump && RandomBool(0.25)) bag << StructureType::Pump;
			// スポナーは時々置く
			if (moneyRed >= CostSpawner && RandomBool(0.30)) bag << StructureType::spawner;

			if (bag.isEmpty()) break;

			const StructureType pick = bag.choice();
			for (int k = 0; k < 100; ++k) {
				const int x = Random(GW / 2 + 1, GW - 2);
				const int y = Random(1, GH - 2);
				if (brd.tiles[brd.idx(x, y)].kind != TileKind::Floor) continue;
				if (brd.redIndex[brd.idx(x, y)] != -1) continue;
				if (brd.blueIndex[brd.idx(x, y)] != -1) continue;

				const float p = brd.tiles[brd.idx(x, y)].paint;
				if (p > 0.45f) continue;

				Structure s; s.owner = Team::Red; s.type = pick; s.cell = { x, y };
				s.hp = GetSpec(pick).maxHP; s.alive = true;
				reds << s;
				brd.redIndex[brd.idx(x, y)] = (int)reds.size() - 1;
				moneyRed -= GetSpec(pick).cost;

				// スポナーを置いたら即座に1体出撃
				if (pick == StructureType::spawner) {
					spawnEnemyAt({ x, y });
				}
				break;
			}
		}
	}

	// ===================== シミュレーション処理 =====================
	void beginSimulation() {
		phase = Phase::Simulating;
		simTime = SimDuration;
		simElapsed = 0.0;
		tracers.clear();
		projectiles.clear();

		auto setup = [&](Array<Structure>& a) {
			for (auto& s : a) {
				if (!s.alive) continue;
				const TypeSpec& spec = GetSpec(s.type);
				if (spec.shots > 0) {
					s.interval = (SimDuration / spec.shots);
					s.nextFire = 0.0;
				}
				else {
					s.interval = 9999.0;
					s.nextFire = 9999.0;
				}
			}
			};
		setup(blues);
		setup(reds);

		// 敵スポナーは独自の出撃タイマーを使う
		for (auto& s : reds) {
			if (s.alive && s.type == StructureType::spawner) {
				s.interval = EnemySpawnerInterval;
				s.nextFire = 0.0; // simElapsed と比較して出撃
			}
		}

		stageStarting = false;
	}

	void endSimulationAndScore() {
		auto [bp, rp] = Ownership(brd);
		const int blueTiles = (int)(bp * GW * GH);
		const int redTiles = (int)(rp * GW * GH);
		moneyBlue += blueTiles * IncomePerTile;
		moneyRed += redTiles * IncomePerTile;

		int bluePump = 0, redPump = 0;
		for (const auto& s : blues) if (s.alive && s.type == StructureType::Pump) ++bluePump;
		for (const auto& s : reds)  if (s.alive && s.type == StructureType::Pump)  ++redPump;
		moneyBlue += bluePump * IncomePerPump;
		moneyRed += redPump * IncomePerPump;

		enemyPlaceAI();

		phase = Phase::Planning;
		turnCount += 1;
	}

	void gotoNextStage() {
		for (int i = 0; i < 10; ++i) {
			SpawnParticles(brd.gridRect.center() + RandomVec2(Circle{ brd.gridRect.h * 0.2 }),
						   HSV{ 120 + Random(-20, 20), 0.8, 1.0 }, 30, 120, 260, 0.4, 0.9, 3, 18);
		}
		moneyBlue = Min(moneyBlue + 60, 9999);
		moneyRed = 140 + 40 * (stage);
		stage += 1;
		buildMapForStage(stage);
		clearShakeAndHitStop();
	}

	// タイル塗り（値の加算、0..1に丸め）
	void applyPaintAt(const Point& c, double delta) {
		if (!brd.inBounds(c.x, c.y)) return;
		Tile& t = brd.tiles[brd.idx(c.x, c.y)];
		const double nv = (double)t.paint + delta;
		t.paint = (float)(nv < 0.0 ? 0.0 : (nv > 1.0 ? 1.0 : nv));
	}

	// 構造体ダメージ（破壊時はグリッドから抜く）
	void damageAt(const Point& c, double dmg, Team attacker) {
		if (!brd.inBounds(c.x, c.y)) return;
		if (attacker == Team::Blue) {
			int idxR = brd.redIndex[brd.idx(c.x, c.y)];
			if (idxR >= 0) {
				Structure& s = reds[idxR];
				if (!s.alive) return;
				s.hp -= dmg;
				if (s.hp <= 0.0) {
					s.alive = false;
					brd.redIndex[brd.idx(c.x, c.y)] = -1;
					applyPaintAt(c, +0.20);
					SpawnParticles(brd.cellCenter(c), HSV{ 0,0.7,1.0 }, 28, 150, 320, 0.30, 0.6, 3, 20);
					AddShake(8.0, 0.15);
					hitStopTimer = Max(hitStopTimer, 0.04);
				}
			}
		}
		else if (attacker == Team::Red) {
			int idxB = brd.blueIndex[brd.idx(c.x, c.y)];
			if (idxB >= 0) {
				Structure& s = blues[idxB];
				if (!s.alive) return;
				s.hp -= dmg;
				if (s.hp <= 0.0) {
					s.alive = false;
					brd.blueIndex[brd.idx(c.x, c.y)] = -1;
					applyPaintAt(c, -0.20);
					SpawnParticles(brd.cellCenter(c), HSV{ 210,0.7,1.0 }, 28, 150, 320, 0.30, 0.6, 3, 20);
					AddShake(8.0, 0.15);
					hitStopTimer = Max(hitStopTimer, 0.04);
				}
			}
		}
	}

	// AoE 適用（半径 r タイル）
	void applyAOE(const Point& center, int r, double paintDelta, double dmg, Team atk) {
		const int r2 = r * r;
		for (int dy = -r; dy <= r; ++dy) for (int dx = -r; dx <= r; ++dx) {
			const int d2 = dx * dx + dy * dy;
			if (d2 > r2) continue;
			Point c{ center.x + dx, center.y + dy };
			if (!brd.inBounds(c.x, c.y)) continue;
			const double w = 1.0 - Sqrt((double)d2) / (double)(r + 0.001);
			applyPaintAt(c, paintDelta * (0.5 + 0.5 * w));
			if (dmg > 0.0) damageAt(c, dmg * (0.6 + 0.4 * w), atk);
		}
	}

	// ターゲット選択（敵タレット優先 / 敵色寄り）
	Optional<Point> findTargetCell(Team atk, const Point& from, int range, bool turretOnly) const {
		Array<Point> cands;
		for (int y = Max(0, from.y - range); y <= Min(GH - 1, from.y + range); ++y) {
			for (int x = Max(0, from.x - range); x <= Min(GW - 1, from.x + range); ++x) {
				const Point p{ x, y };
				if (TileDist(from, p) > (double)range + 0.001) continue;
				const Tile& t = brd.tiles[brd.idx(x, y)];
				if (turretOnly) {
					if (atk == Team::Blue) {
						if (brd.redIndex[brd.idx(x, y)] >= 0) cands << p;
					}
					else {
						if (brd.blueIndex[brd.idx(x, y)] >= 0) cands << p;
					}
				}
				else {
					const bool enemyish = (atk == Team::Blue ? (t.paint < 0.45f) : (t.paint > 0.55f));
					if (enemyish) cands << p;
				}
			}
		}
		if (!cands.isEmpty()) return cands.choice();

		Array<Point> any;
		for (int y = Max(0, from.y - range); y <= Min(GH - 1, from.y + range); ++y) {
			for (int x = Max(0, from.x - range); x <= Min(GW - 1, from.x + range); ++x) {
				const Point p{ x, y };
				if (TileDist(from, p) > (double)range + 0.001) continue;
				if (brd.tiles[brd.idx(x, y)].kind == TileKind::Wall) continue;
				any << p;
			}
		}
		if (!any.isEmpty()) return any.choice();
		return none;
	}

	// 弾を登録（マズルフラッシュ/薬莢/トレーサーなどの演出もここ）
	void spawnProjectile(Team atk, const TypeSpec& spec, const Vec2& muzzle, const Point& targetCell, ProjKind k, bool useArc, bool blocked, bool indirect, int aoe, double dmg, double paint, double speed, double radiusPx) {
		Projectile pr;
		pr.kind = k;
		pr.owner = atk;
		pr.blockedByWalls = blocked;
		pr.indirect = indirect;
		pr.aoeRadius = aoe;
		pr.damage = dmg;
		pr.paint = paint;
		pr.radius = radiusPx;
		pr.targetCell = targetCell;
		pr.pos = muzzle;
		pr.life = 3.0;

		const Vec2 hitPos = brd.cellCenter(targetCell);

		if (useArc) {
			pr.useArc = true;
			pr.startPos = muzzle;
			pr.endPos = hitPos;
			const Vec2 mid = (muzzle + hitPos) * 0.5;
			const double dist = (hitPos - muzzle).length();
			const double h = 60.0 + 0.25 * dist; // 距離に応じて高く
			pr.apexPos = mid + Vec2{ 0, -h };
			pr.u = 0.0;
			pr.pathLen = dist;
			pr.pathSpeed = speed;
		}
		else {
			pr.useArc = false;
			Vec2 dir = (hitPos - muzzle);
			const double len = dir.length();
			if (len > 0.0) dir *= (speed / len);
			else dir = Vec2{ 0,0 };
			pr.vel = dir;
		}

		projectiles << pr;

		// マズルフラッシュ＆発射煙
		SpawnParticles(muzzle, TeamColor(atk), 6, 120, 260, 0.08, 0.22, 3, 10);
		tracers << Tracer{ muzzle, muzzle + (hitPos - muzzle).setLength(18.0), TeamColor(atk).withAlpha(0.9), 0.0, 0.12 };
	}

	// 1発発射（弾を生成する）
	void fireOnce(Structure& s, Team atk) {
		const TypeSpec& spec = GetSpec(s.type);
		if (spec.shots <= 0) return;

		const Vec2 muzzle = brd.cellCenter(s.cell);

		if (s.type == StructureType::Sprinkler) {
			// 自身の周囲に小弾を複数散布
			for (int i = 0; i < 3; ++i) {
				Point tc = s.cell + Point{ Random(-spec.range, spec.range), Random(-spec.range, spec.range) };
				tc.x = limit(tc.x, 0, GW - 1);
				tc.y = limit(tc.y, 0, GH - 1);
				spawnProjectile(atk, spec, muzzle, tc,
					ProjKind::Droplet, false, false, false, 0, spec.damage * 0.15, spec.paint * 0.9, spec.projSpeed, spec.projRadius * 0.9);
			}
			return;
		}

		Optional<Point> opt = findTargetCell(atk, s.cell, spec.range, spec.targetTurretOnly);
		if (!opt) return;
		Point target = *opt;

		// ブレ（Mortar / Basic）
		if (spec.spread > 0.05) {
			target.x += Random(-(int)spec.spread, (int)spec.spread);
			target.y += Random(-(int)spec.spread, (int)spec.spread);
			target.x = limit(target.x, 0, GW - 1);
			target.y = limit(target.y, 0, GH - 1);
		}

		if (s.type == StructureType::Mortar) {
			spawnProjectile(atk, spec, muzzle, target,
				ProjKind::Mortar, true, false, true, spec.aoeRadius, spec.damage, spec.paint, spec.projSpeed, spec.projRadius);
		}
		else if (s.type == StructureType::Sniper) {
			spawnProjectile(atk, spec, muzzle, target,
				ProjKind::Sniper, false, true, false, 0, spec.damage, spec.paint, spec.projSpeed, spec.projRadius);
		}
		else { // Basic
			spawnProjectile(atk, spec, muzzle, target,
				ProjKind::Bullet, false, true, false, 0, spec.damage, spec.paint, spec.projSpeed, spec.projRadius);
		}
	}

	// 弾の進行・衝突処理
	void updateProjectiles(double dtReal) {
		const double dt = dtReal;

		Array<Projectile> alive;

		for (auto& pr : projectiles) {
			pr.age += dt;
			if (pr.age > pr.life) {
				continue; // 消滅
			}

			Vec2 prev = pr.pos;

			// 放物線 or 直進
			if (pr.useArc) {
				// 経路パラメータ更新
				const double du = (pr.pathSpeed / Max(1.0, pr.pathLen)) * dt;
				pr.u += du;
				double u = pr.u; if (u > 1.0) u = 1.0;
				const double v = (1.0 - u);
				pr.pos = (v * v) * pr.startPos + (2.0 * v * u) * pr.apexPos + (u * u) * pr.endPos;

				// 軌跡
				tracers << Tracer{ prev, pr.pos, TeamColor(pr.owner), 0.0, 0.10 };

				// 到達
				if (pr.u >= 1.0) {
					const Point ic = brd.screenToCell(pr.endPos).value_or(pr.targetCell);
					impactAt(pr, ic);
					continue;
				}
				alive << pr;
			}
			else {
				// 直進（衝突はサブステップで検出）
				Vec2 remain = pr.vel * dt;
				int subSteps = Max(1, (int)Ceil(remain.length() / (brd.tileSize * 0.5)));
				Vec2 step = remain / (double)subSteps;
				bool hit = false;
				for (int i = 0; i < subSteps; ++i) {
					prev = pr.pos;
					pr.pos += step;

					// 壁衝突判定（貫通不可のみ）
					if (pr.blockedByWalls && !pr.indirect) {
						if (const auto oc = brd.posToCell(pr.pos)) {
							const TileKind k = brd.tiles[brd.idx(oc->x, oc->y)].kind;
							if (k == TileKind::Wall) {
								// 一つ前の位置で停止、そこを着弾に
								const Point ic = brd.posToCell(prev).value_or(*oc);
								impactAt(pr, ic);
								hit = true;
								break;
							}
						}
						else {
							// 盤面外
							hit = true; break;
						}
					}
					// 目標セル到達チェック（近似）
					if (const auto tc = brd.posToCell(pr.pos)) {
						if (tc->x == pr.targetCell.x && tc->y == pr.targetCell.y) {
							impactAt(pr, *tc);
							hit = true;
							break;
						}
					}
					// トレイル
					tracers << Tracer{ prev, pr.pos, TeamColor(pr.owner), 0.0, 0.08 };
				}
				if (!hit) {
					// 盤面外チェック
					if (!brd.gridRect.intersects(pr.pos)) {
						// 消滅
					}
					else {
						alive << pr;
					}
				}
			}
		}

		projectiles.swap(alive);
	}

	// 着弾時の効果
	void impactAt(const Projectile& pr, const Point& ic) {
		if (!brd.inBounds(ic.x, ic.y)) return;

		// AoE or 単体
		if (pr.aoeRadius > 0) {
			applyAOE(ic, pr.aoeRadius, (pr.owner == Team::Blue ? +pr.paint : -pr.paint), pr.damage, pr.owner);
			SpawnParticles(brd.cellCenter(ic), (pr.owner == Team::Blue ? HSV{ 210,0.8,1.0 } : HSV{ 0,0.8,1.0 }), 18, 140, 260, 0.25, 0.6, 4, 18);
			Circle{ brd.cellCenter(ic), 16.0 + pr.aoeRadius * brd.tileSize * 0.25 }.drawFrame(3, TeamColor(pr.owner).withAlpha(0.6));
			AddShake(7.0, 0.12);
			hitStopTimer = Max(hitStopTimer, 0.02);
		}
		else {
			applyPaintAt(ic, (pr.owner == Team::Blue ? +pr.paint : -pr.paint));
			damageAt(ic, pr.damage, pr.owner);
			// ヒットスパーク
			SpawnParticles(brd.cellCenter(ic), (pr.owner == Team::Blue ? HSV{ 210,0.8,1.0 } : HSV{ 0,0.8,1.0 }), 10, 120, 220, 0.20, 0.45, 3, 12);
			AddShake(3.0, 0.06);
		}
	}

	// ===================== プレイヤー操作・敵AI（体当たり） =====================

	// 主に盤面外や壁への侵入を防ぐ
	bool isWallAt(const Vec2& p) const {
		if (const auto oc = brd.posToCell(p)) {
			return (brd.tiles[brd.idx(oc->x, oc->y)].kind == TileKind::Wall);
		}
		return true; // 盤面外は壁扱い
	}

	void moveWithCollide(Actor& a, const Vec2& delta) {
		if (!a.alive) return;

		// X方向
		Vec2 np = a.pos + Vec2{ delta.x, 0 };
		const double minX = brd.gridRect.x + a.radius;
		const double maxX = brd.gridRect.x + brd.gridRect.w - a.radius;
		np.x = Clamp(np.x, minX, maxX);
		if (!isWallAt(np)) a.pos.x = np.x;

		// Y方向
		np = a.pos + Vec2{ 0, delta.y };
		const double minY = brd.gridRect.y + a.radius;
		const double maxY = brd.gridRect.y + brd.gridRect.h - a.radius;
		np.y = Clamp(np.y, minY, maxY);
		if (!isWallAt(np)) a.pos.y = np.y;
	}

	// スポナーをクリックで出撃（プラン/シミュどちらでも可）
	bool trySpawnFromClickedSpawner() {
		if (!MouseL.down()) return false;
		if (const auto oc = brd.screenToCell(Cursor::PosF())) {
			const int bi = brd.blueIndex[brd.idx(oc->x, oc->y)];
			if (bi >= 0) {
				const Structure& s = blues[bi];
				if (s.alive && s.type == StructureType::spawner) {
					spawnPlayerAt(s.cell);
					SpawnParticles(brd.cellCenter(s.cell), HSV{ 210,0.8,1.0 }, 14, 120, 240, 0.18, 0.35, 3, 12);
					return true;
				}
			}
		}
		return false;
	}

	void spawnPlayerAt(const Point& c) {
		Actor a;
		a.pos = brd.cellCenter(c);
		a.lastPos = a.pos;
		a.radius = PlayerRadius;
		a.speed = PlayerSpeed;
		a.hp = HPPlayer;
		a.alive = true;
		player = a;
	}

	// プレイヤー爆発（AoE+強いカメラシェイク）
	void playerExplodeAt(const Point& cell) {
		const Vec2 center = brd.cellCenter(cell);

		// 被害（塗り+ダメージ）
		applyAOE(cell, PlayerExplodeRadius, +PlayerExplodePaint, PlayerExplodeDamage, Team::Blue);

		// 視覚効果（強め）
		SpawnParticles(center, HSV{ 210,0.85,1.0 }, 36, 180, 360, 0.30, 0.70, 5, 22);
		SpawnParticles(center, ColorF{ 1.0, 0.95 }, 18, 120, 260, 0.12, 0.25, 4, 14);

		AddShake(PlayerExplodeShakePow, PlayerExplodeShakeDur);
		hitStopTimer = Max(hitStopTimer, PlayerExplodeHitstop);

		// 自爆扱い
		if (player) {
			player->alive = false;
		}
	}

	// 移動経路のセルを塗る（1フレーム分の合計塗り量を経路に等分配）
	void paintTrailByMove(const Vec2& from, const Vec2& to, double dt) {
		const auto c0 = brd.posToCell(from);
		const auto c1 = brd.posToCell(to);
		if (!c0 || !c1) return;

		Array<Point> path = LineCells(*c0, *c1);
		if (path.isEmpty()) return;

		const double total = PlayerPaintPerSecond * dt;
		const double per = total / (double)path.size();

		for (const auto& cell : path) {
			// 壁は塗らない
			if (brd.inBounds(cell.x, cell.y) && brd.tiles[brd.idx(cell.x, cell.y)].kind != TileKind::Wall) {
				applyPaintAt(cell, +per);
			}
		}
	}

	// 毎フレームのプレイヤー更新（WASD移動・塗り・体当たり爆発）
	void updatePlayer(double dt) {
		// クリックでスポナーから出撃（先に処理）
		trySpawnFromClickedSpawner();

		if (!player || !player->alive) return;

		Vec2 dir{ 0,0 };
		if (KeyW.pressed()) dir.y -= 1;
		if (KeyS.pressed()) dir.y += 1;
		if (KeyA.pressed()) dir.x -= 1;
		if (KeyD.pressed()) dir.x += 1;

		Vec2 prev = player->pos;

		if (dir.lengthSq() > 0.0) {
			dir = dir.setLength(player->speed * dt);
			moveWithCollide(*player, dir);
		}

		// 移動経路を塗る
		paintTrailByMove(prev, player->pos, dt);

		// 敵構造物に接触したら爆発
		if (const auto oc = brd.posToCell(player->pos)) {
			const int idxR = brd.redIndex[brd.idx(oc->x, oc->y)];
			if (idxR >= 0) {
				playerExplodeAt(*oc);
				return; // このフレームの処理はここまで
			}
		}

		// 位置更新
		player->lastPos = player->pos;
	}

	// ===== 敵ユニット（AI） =====

	// 敵ユニット生成
	void spawnEnemyAt(const Point& c) {
		Actor a;
		a.pos = brd.cellCenter(c);
		a.lastPos = a.pos;
		a.radius = EnemyRadius;
		a.speed = EnemySpeed;
		a.hp = HPEnemy;
		a.alive = true;
		redAgents << a;
		SpawnParticles(a.pos, HSV{ 0,0.8,1.0 }, 10, 100, 200, 0.15, 0.30, 3, 12);
	}

	// 敵爆発
	void enemyExplodeAt(const Point& cell) {
		const Vec2 center = brd.cellCenter(cell);
		applyAOE(cell, EnemyExplodeRadius, -EnemyExplodePaint, EnemyExplodeDamage, Team::Red);
		SpawnParticles(center, HSV{ 0,0.85,1.0 }, 28, 160, 320, 0.25, 0.60, 5, 20);
		AddShake(EnemyExplodeShakePow, EnemyExplodeShakeDur);
		hitStopTimer = Max(hitStopTimer, EnemyExplodeHitstop);
	}

	// 敵の目標（最も近い味方構造物中心）
	Vec2 enemySeekTargetVec(const Actor& e) const {
		double bestD2 = 1e18;
		Vec2 best{ 0,0 };
		for (const auto& s : blues) {
			if (!s.alive) continue;
			Vec2 p = brd.cellCenter(s.cell);
			double d2 = (p - e.pos).lengthSq();
			if (d2 < bestD2) { bestD2 = d2; best = (p - e.pos); }
		}
		return best;
	}

	// 敵スポナーの定期出撃
	void updateEnemySpawnerProduction(double dt) {
		(void)dt;
		// simElapsed と nextFire を比較して出撃
		for (auto& s : reds) {
			if (!s.alive || s.type != StructureType::spawner) continue;
			while (simElapsed + 1e-6 >= s.nextFire) {
				spawnEnemyAt(s.cell);
				s.nextFire += s.interval; // interval は beginSimulation で設定済み
			}
		}
	}

	// 敵ユニット更新（移動→接触で爆発）
	void updateRedAgents(double dt) {
		Array<Actor> stillAlive;
		stillAlive.reserve(redAgents.size());

		for (auto& e : redAgents) {
			if (!e.alive) continue;

			// ターゲットへ直進（簡易）
			Vec2 to = enemySeekTargetVec(e);
			if (to.lengthSq() > 1e-4) {
				Vec2 step = to.setLength(e.speed * dt);
				moveWithCollide(e, step);
			}

			// 味方（Blue）構造物に接触したら爆発
			bool exploded = false;
			if (const auto oc = brd.posToCell(e.pos)) {
				const int idxB = brd.blueIndex[brd.idx(oc->x, oc->y)];
				if (idxB >= 0) {
					enemyExplodeAt(*oc);
					e.alive = false;
					exploded = true;
				}
			}

			if (e.alive && !exploded) {
				stillAlive << e;
			}
		}

		redAgents.swap(stillAlive);
	}

	// シミュレーション更新
	void updateSimulation(double dtReal) {
		// ヒットストップ -> timeScale
		if (hitStopTimer > 0.0) { hitStopTimer -= dtReal; timeScale = (hitStopTimer <= 0.0 ? 1.0 : 0.0); }
		else { timeScale = 1.0; }
		const double dt = dtReal * timeScale;

		simElapsed += dt;
		simTime -= dt;
		if (simTime <= 0.0) {
			simTime = 0.0;
			phase = Phase::Summary;
			stageCleared = isBlueWin() && !isBlueLose();
			clearShakeAndHitStop();
		}

		// 発射スケジュール
		auto stepFire = [&](Array<Structure>& a, Team atk) {
			for (auto& s : a) {
				if (!s.alive) continue;
				const TypeSpec& spec = GetSpec(s.type);
				if (spec.shots <= 0) continue;
				while (simElapsed + 1e-6 >= s.nextFire) {
					fireOnce(s, atk);
					s.nextFire += s.interval;
				}
			}
			};
		stepFire(blues, Team::Blue);
		stepFire(reds, Team::Red);

		// 実弾・トレーサー・パーティクル
		updateProjectiles(dt);

		// 敵スポナー出撃
		updateEnemySpawnerProduction(dt);

		// 敵ユニット
		updateRedAgents(dt);

		// プレイヤー
		updatePlayer(dt);

		Array<Tracer> aliveT;
		for (auto& t : tracers) {
			t.age += dtReal;
			if (t.age < t.life) aliveT << t;
		}
		tracers.swap(aliveT);

		Array<Particle> aliveP;
		for (auto& p : particles) {
			p.age += dtReal;
			p.pos += p.vel * dtReal;
			p.vel *= 0.98;
			if (p.age < p.life) aliveP << p;
		}
		particles.swap(aliveP);

		// HQ 勝敗判定（中断）
		if (isBlueLose()) {
			phase = Phase::Summary;
			stageCleared = false;
			clearShakeAndHitStop();
		}
		else if (isBlueWin()) {
			phase = Phase::Summary;
			stageCleared = true;
			clearShakeAndHitStop();
		}
	}

	// サマリー->次ターン or 次ステージ（Enterでも進行）
	void updateSummary() {
		const double panelY = brd.gridRect.y + brd.gridRect.h * 0.60;
		const RectF panel{ brd.gridRect.x, panelY, brd.gridRect.w, 160 };
		panel.draw(ColorF{ 0,0,0,0.6 });
		panel.drawFrame(2, ColorF{ 0,0,0,0.8 });

		String msg;
		if (isBlueLose())       msg = U"敗北…";
		else if (isBlueWin())   msg = U"ステージクリア！";
		else                    msg = U"ターン終了";

		FontAsset(U"UI")(U"結果: {}"_fmt(msg)).drawAt(32, panel.center().movedBy(0, -36), ColorF{ 1 });
		const bool enter = KeyEnter.down();

		if (isBlueWin()) {
			const bool clicked = SimpleGUI::Button(U"次のステージへ [Enter]", Vec2{ panel.center().x - 160, panel.center().y - 10 }, 220);
			if (clicked || enter) { gotoNextStage(); return; }
		}
		else if (isBlueLose()) {
			const bool clicked = SimpleGUI::Button(U"ステージ再挑戦 [Enter]", Vec2{ panel.center().x - 120, panel.center().y - 10 }, 240);
			if (clicked || enter) { buildMapForStage(stage); return; }
		}
		else {
			const bool clicked = SimpleGUI::Button(U"次ターンへ（収益計算） [Enter]", Vec2{ panel.center().x - 160, panel.center().y - 10 }, 320);
			if (clicked || enter) { endSimulationAndScore(); return; }
		}
		FontAsset(U"UI")(U"Enter ですすむ / クリックでも可").drawAt(20, panel.center().movedBy(0, 36), ColorF{ 0.95 });
	}

	// 入力（プランニング）
	void updatePlanning() {
		// 先に「スポナーをクリックで出撃」を判定
		if (trySpawnFromClickedSpawner()) {
			// 出撃クリックの場合は配置処理をスキップ
		}
		else if (MouseL.down()) {
			// クリックで配置（スポナー含む）
			if (const auto oc = brd.screenToCell(Cursor::PosF())) {
				String reason;
				if (canPlace(Team::Blue, selectedType, *oc, reason)) {
					placeBlue(selectedType, *oc);
					SpawnParticles(brd.cellCenter(*oc), HSV{ 210,0.8,1.0 }, 10, 90, 180, 0.2, 0.45, 2, 10);
				}
			}
		}

		// Enterで自動戦闘開始
		if (KeyEnter.down()) {
			beginSimulation();
		}

		// プランニング中もプレイヤーは操作できる（移動で塗れる）
		updatePlayer(Scene::DeltaTime());

		// ステージ開始バナー進行
		if (stageStarting) {
			stageBannerT = Min(1.0, stageBannerT + Scene::DeltaTime() / 1.2);
		}
	}

	// 置けるか判定
	bool canPlace(Team side, StructureType type, const Point& c, String& reason) const {
		if (!brd.inBounds(c.x, c.y)) { reason = U"範囲外"; return false; }
		const Tile& t = brd.tiles[brd.idx(c.x, c.y)];
		if (t.kind != TileKind::Floor) { reason = U"ここには置けません"; return false; }

		if (side == Team::Blue) {
			if (brd.blueIndex[brd.idx(c.x, c.y)] != -1) { reason = U"味方構造物あり"; return false; }
			if (brd.redIndex[brd.idx(c.x, c.y)] != -1) { reason = U"敵構造物あり"; return false; }
		}
		else {
			if (brd.redIndex[brd.idx(c.x, c.y)] != -1) { reason = U"自軍構造物あり"; return false; }
			if (brd.blueIndex[brd.idx(c.x, c.y)] != -1) { reason = U"敵構造物あり"; return false; }
		}

		const bool own = (side == Team::Blue ? (t.paint > 0.55f) : (t.paint < 0.45f));
		if (!own) { reason = U"自軍インク外"; return false; }

		const int cost = GetSpec(type).cost;
		if (side == Team::Blue) {
			if (moneyBlue < cost) { reason = U"$不足"; return false; }
		}
		else {
			if (moneyRed < cost) { reason = U"$不足(敵)"; return false; }
		}

		reason = U"OK";
		return true;
	}

	// プレイヤー設置
	bool placeBlue(StructureType type, const Point& c) {
		String r; if (!canPlace(Team::Blue, type, c, r)) return false;
		Structure s; s.owner = Team::Blue; s.type = type; s.cell = c; s.hp = GetSpec(type).maxHP; s.alive = true;
		blues << s;
		brd.blueIndex[brd.idx(c.x, c.y)] = (int)blues.size() - 1;
		moneyBlue -= GetSpec(type).cost;

		// スポナーは出撃ポイント。出撃はクリック操作で
		return true;
	}

	// ===================== 描画 =====================
	void drawBoard() const {
		for (int y = 0; y < GH; ++y) {
			for (int x = 0; x < GW; ++x) {
				const Tile& t = brd.tiles[brd.idx(x, y)];
				ColorF color;
				const double s = (t.paint - 0.5) * 2.0; // -1..1
				if (s >= 0) color = TeamColor(Team::Blue).lerp(ColorF{ 1.0 }, 1.0 - s);
				else        color = TeamColor(Team::Red).lerp(ColorF{ 1.0 }, 1.0 - (-s));

				RectF rc = brd.cellRect(Point{ x, y });
				rc.draw(color);
				rc.drawFrame(1, ColorF{ 0,0,0,0.15 });

				if (t.kind == TileKind::Wall) {
					rc.stretched(-2).draw(ColorF{ 0.12, 0.12, 0.13 });
				}
				else if (t.kind == TileKind::HQBlue) {
					Circle{ rc.center(), rc.w * 0.38 }.draw(HSV{ 210, 0.6, 1.0 });
				}
				else if (t.kind == TileKind::HQRed) {
					Circle{ rc.center(), rc.w * 0.38 }.draw(HSV{ 0, 0.6, 1.0 });
				}
			}
		}
	}

	void drawStructures() const {
		auto drawSide = [&](const Array<Structure>& a) {
			for (const auto& s : a) {
				if (!s.alive) continue;
				const RectF rc = brd.cellRect(s.cell).stretched(-4);
				const ColorF base = (s.owner == Team::Blue ? HSV{ 210,0.9,1.0 } : HSV{ 0,0.9,1.0 });

				switch (s.type) {
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
					// 歯車っぽいスポナー
					Circle{ rc.center(), rc.w * 0.36 }.draw(base);
					for (int i = 0; i < 6; ++i) {
						const double ang = i * (Math::TwoPi / 6.0);
						Vec2 p = rc.center() + Vec2{ Cos(ang), Sin(ang) } *rc.w * 0.55;
						Circle{ p, rc.w * 0.10 }.draw(base);
					}
					Circle{ rc.center(), rc.w * 0.18 }.draw(ColorF{ 0,0,0,0.75 });
					rc.drawFrame(2, ColorF{ 0,0,0,0.65 });
					break;
				}

				// HPバー
				const double maxHP = GetSpec(s.type).maxHP;
				if (maxHP > 0.0 && s.type != StructureType::HQ) {
					const double ratio = (s.hp / maxHP);
					const double rr = (ratio < 0.0 ? 0.0 : (ratio > 1.0 ? 1.0 : ratio));
					const RectF hb{ rc.x, rc.y - 6, rc.w, 4 };
					hb.draw(ColorF{ 0,0,0,0.5 });
					RectF{ hb.pos, hb.w * rr, hb.h }.draw((s.owner == Team::Blue) ? ColorF{ 0.2,0.9,0.3 } : ColorF{ 0.9,0.2,0.2 });
				}
			}
			};
		drawSide(blues);
		drawSide(reds);
	}

	void drawTracers() const {
		for (const auto& t : tracers) {
			const double a = 1.0 - (t.age / t.life);
			Line{ t.p0, t.p1 }.draw(4, t.col.withAlpha(0.35 * a));
			Line{ t.p0, t.p1 }.draw(2, ColorF{ 1.0, a * 0.8 });
		}
	}

	void drawParticles() const {
		for (const auto& p : particles) {
			const double t = p.age / p.life;
			const double r = p.size0 + (p.size1 - p.size0) * t;
			const double a = 1.0 - t;
			Circle{ p.pos, r }.draw(p.col.withAlpha(0.6 * a));
		}
	}

	void drawProjectiles() const {
		for (const auto& pr : projectiles) {
			ColorF c = TeamColor(pr.owner);
			if (pr.kind == ProjKind::Sniper) {
				// スナイパーは光る弾
				Circle{ pr.pos, pr.radius * 0.8 }.draw(ColorF{ 1.0, 0.95 });
				Circle{ pr.pos, pr.radius * 1.6 }.drawFrame(2, c.withAlpha(0.8));
			}
			else if (pr.kind == ProjKind::Mortar) {
				// 迫撃砲はコア＋外縁
				Circle{ pr.pos, pr.radius }.draw(c.withAlpha(0.9));
				Circle{ pr.pos, pr.radius * 1.6 }.drawFrame(2, ColorF{ 0,0,0,0.35 });
			}
			else {
				Circle{ pr.pos, pr.radius }.draw(c);
			}
		}
	}

	// プレイヤー描画
	void drawPlayer() const {
		if (player && player->alive) {
			Circle{ player->pos, player->radius }.draw(HSV{ 210, 0.9, 1.0 });
			Circle{ player->pos, player->radius + 3 }.drawFrame(2, ColorF{ 1,1,1,0.6 });
		}
	}

	// 敵ユニット描画
	void drawEnemies() const {
		for (const auto& e : redAgents) {
			if (!e.alive) continue;
			Circle{ e.pos, e.radius }.draw(HSV{ 0, 0.9, 1.0 });
			Circle{ e.pos, e.radius + 2 }.drawFrame(2, ColorF{ 0,0,0,0.6 });
		}
	}

	// UI（選択ボタンで selectedType を変更するため非const）
	void drawUI() {
		const RectF ui{ Scene::Width() - UIWidth + Margin * 0.5, Margin, UIWidth - Margin * 1.5, Scene::Height() - 2 * Margin };
		ui.draw(ColorF{ 0,0,0,0.25 });
		ui.drawFrame(2, ColorF{ 0,0,0,0.4 });

		const String ph =
			(phase == Phase::Planning) ? U"フェーズ: 設置" :
			(phase == Phase::Simulating) ? U"フェーズ: 自動戦闘" : U"フェーズ: 結果";
		FontAsset(U"UI")(U"インクウォーズ（仮）").draw(24, Vec2{ ui.x + 14, ui.y + 10 }, ColorF{ 1 });
		FontAsset(U"UI")(U"ステージ {}"_fmt(stage)).draw(20, Vec2{ ui.x + 14, ui.y + 42 }, ColorF{ 1 });
		FontAsset(U"UI")(ph).draw(18, Vec2{ ui.x + 14, ui.y + 68 }, ColorF{ 1 });

		FontAsset(U"UI")(U"Blue $: {}"_fmt(moneyBlue)).draw(20, Vec2{ ui.x + 14, ui.y + 92 }, ColorF{ 1 });

		auto [bp, rp] = Ownership(brd);
		RectF rbb{ ui.x + 14, ui.y + 120, ui.w - 28, 14 };
		rbb.draw(ColorF{ 0,0,0,0.3 });
		RectF{ rbb.pos, rbb.w * (bp < 0.0 ? 0.0 : (bp > 1.0 ? 1.0 : bp)), rbb.h }.draw(HSV{ 210,0.8,1.0 });
		RectF{ rbb.pos.movedBy(0, 18), rbb.w * (rp < 0.0 ? 0.0 : (rp > 1.0 ? 1.0 : rp)), rbb.h }.draw(HSV{ 0,0.8,1.0 });
		FontAsset(U"UI")(U"BLUE 支配: {:.0f}%  /  RED 支配: {:.0f}%"_fmt(bp * 100.0, rp * 100.0)).draw(18, Vec2{ ui.x + 14, ui.y + 152 }, ColorF{ 0.95 });

		const auto drawButton = [&](const String& label, StructureType t, double y, int cost) {
			RectF b{ ui.x + 14, y, ui.w - 28, 36 };
			const bool sel = (selectedType == t);
			b.draw(sel ? ColorF{ 0.2,0.35,0.6, 0.7 } : ColorF{ 0,0,0,0.25 });
			b.drawFrame(1, ColorF{ 0,0,0,0.5 });
			FontAsset(U"UI")(U"{}  ${}"_fmt(label, cost)).draw(20, b.pos.movedBy(8, 8), ColorF{ 1 });
			if (phase == Phase::Planning && b.leftClicked()) selectedType = t;
			};

		double y = ui.y + 186;
		drawButton(U"基本タレット", StructureType::Basic, y += 42, CostBasic);
		drawButton(U"スプリンクラー", StructureType::Sprinkler, y += 42, CostSprinkler);
		drawButton(U"インクポンプ", StructureType::Pump, y += 42, CostPump);
		drawButton(U"スナイパー", StructureType::Sniper, y += 42, CostSniper);
		drawButton(U"迫撃砲", StructureType::Mortar, y += 42, CostMortar);
		drawButton(U"スポナー", StructureType::spawner, y += 42, CostSpawner);

		y += 60;
		if (phase == Phase::Planning) {
			FontAsset(U"UI")(U"[Click] 置く / スポナー[Click]出撃 / [Enter] 自動戦闘 10s").draw(18, Vec2{ ui.x + 14, y }, ColorF{ 0.95 });
			FontAsset(U"UI")(U"[WASD] で移動して塗る / 敵はAIでスポーンし体当たり").draw(16, Vec2{ ui.x + 14, y + 24 }, ColorF{ 0.95 });
		}
		else if (phase == Phase::Simulating) {
			FontAsset(U"UI")(U"自動戦闘中… 残り {:.1f}s"_fmt(simTime)).draw(18, Vec2{ ui.x + 14, y }, ColorF{ 0.95 });
			FontAsset(U"UI")(U"敵はスポナーから定期出撃 → Blue構造物へ体当たり").draw(16, Vec2{ ui.x + 14, y + 24 }, ColorF{ 0.95 });
		}
		else {
			FontAsset(U"UI")(U"[Enter] でも次へ進めます").draw(18, Vec2{ ui.x + 14, y }, ColorF{ 0.95 });
		}

		y += 48;
		FontAsset(U"UI")(U"ターン {}"_fmt(turnCount)).draw(20, Vec2{ ui.x + 14, y }, ColorF{ 1 });

		// プレイヤーの状態と敵ユニット数
		if (player && player->alive) {
			FontAsset(U"UI")(U"Player HP: {:.0f}/{}"_fmt(player->hp, HPPlayer)).draw(18, Vec2{ ui.x + 14, y + 26 }, ColorF{ 1 });
		}
		FontAsset(U"UI")(U"敵ユニット数: {}"_fmt(redAgents.count_if([](const Actor& a) { return a.alive; }))).draw(18, Vec2{ ui.x + 14, y + 50 }, ColorF{ 1 });
	}

	void drawHoverHelp() const {
		if (phase != Phase::Planning) return;
		if (const auto oc = brd.screenToCell(Cursor::PosF())) {
			const Point c = *oc;
			const RectF rc = brd.cellRect(c).stretched(-2);

			// スポナー上にマウスがあるときは出撃ヒント
			const int bi = brd.blueIndex[brd.idx(c.x, c.y)];
			if (bi >= 0 && blues[bi].alive && blues[bi].type == StructureType::spawner) {
				rc.drawFrame(3, ColorF{ 0.2,0.9,0.4,0.9 });
				FontAsset(U"UI")(U"[Click] 出撃").draw(16, rc.pos.movedBy(2, 2), ColorF{ 1 });
				return;
			}

			String reason;
			const bool ok = canPlace(Team::Blue, selectedType, c, reason);
			rc.drawFrame(3, ok ? ColorF{ 0.2,0.9,0.4,0.9 } : ColorF{ 0.9,0.2,0.2, 0.9 });

			const TypeSpec& sp = GetSpec(selectedType);
			if (sp.range > 0) {
				const double r = sp.range * brd.tileSize;
				Circle{ brd.cellCenter(c), r }.drawFrame(2, ColorF{ 1,1,1,0.25 });
			}
		}
	}

	void drawStageBanner() const {
		if (!stageStarting && phase != Phase::Summary) return;
		double t = stageStarting ? stageBannerT : 1.0;
		const double alpha = easeInOutSine(Min(1.0, t));
		const String text = (phase == Phase::Summary && stageCleared) ? U"STAGE CLEAR!" : U"STAGE " + Format(stage);
		const Vec2 c = brd.gridRect.center();
		const double w = brd.gridRect.w * 0.7;
		const double h = 64;
		const double yOff = (1.0 - easeOutBack(alpha)) * -80.0;
		const RectF banner{ c.x - w / 2, c.y - h / 2 + yOff, w, h };
		banner.draw(ColorF{ 0,0,0, 0.55 * alpha });
		banner.drawFrame(2, ColorF{ 1,1,1, 0.8 * alpha });
		FontAsset(U"UI")(text).drawAt(32, banner.center(), ColorF{ 1,1,1, alpha });
	}
};

// ===================== メイン =====================
void Main() {
	Window::Resize(1600, 900);
	Scene::SetBackground(ColorF{ 0.06, 0.06, 0.07 });
	FontAsset::Register(U"UI", FontMethod::MSDF, 20, Typeface::Bold);
	const ScopedRenderStates2D _sampler{ SamplerState::ClampLinear };

	Game G;
	G.layout();
	G.buildMapForStage(1);

	while (System::Update()) {
		const double dtReal = Scene::DeltaTime();

		// レイアウトはリサイズに追従
		G.layout();

		// 毎フレーム、演出の減衰を回す（どのフェーズでも）
		G.updateEffectsEveryFrame(dtReal);

		// 入力・更新
		if (G.phase == Phase::Planning) {
			G.updatePlanning();
		}
		else if (G.phase == Phase::Simulating) {
			G.updateSimulation(dtReal);
			// 自動フェーズのスキップ（サマリー突入時に演出停止）
			if (SimpleGUI::Button(U"スキップ", Vec2{ Scene::Width() - UIWidth + 24, Scene::Height() - 48 }, 120)) {
				G.phase = Phase::Summary;
				G.stageCleared = G.isBlueWin() && !G.isBlueLose();
				G.clearShakeAndHitStop();
			}
		}
		else {
			G.updateSummary();
		}

		// 盤面のみシェイクを適用（UIは適用しない）
		{
			const Transformer2D _tr(Mat3x2::Translate(G.GetShakeOffset()), TransformCursor::No);
			G.drawBoard();
			G.drawStructures();
			G.drawTracers();
			G.drawParticles();
			G.drawProjectiles();
			G.drawPlayer();
			G.drawEnemies();
		}
		// UI・ヘルプ・バナーはシェイクなしで描画（クリック判定のズレ防止）
		G.drawUI();
		G.drawHoverHelp();
		G.drawStageBanner();

		// 参考表示
		auto [bp, rp] = Ownership(G.brd);
		const bool blueLose = G.isBlueLose();
		const bool blueWin = G.isBlueWin();
		if (blueLose || blueWin) {
			const String msg = blueLose ? U"敗北条件達成" : U"勝利条件達成";
			FontAsset(U"UI")(msg).drawAt(28, Vec2{ Scene::Width() * 0.5, 26 }, ColorF{ 1, 1, 0.8 });
		}
	}
}
