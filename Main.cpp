# include <Siv3D.hpp> // Siv3D v0.6.16

void Main()
{
	// 基本設定
	Scene::SetBackground(ColorF{ 0.98 });
	Window::Resize(1920, 1080);
	constexpr Size CellSize{ 40, 40 };
	const Size gridCount{ (Scene::Width() / CellSize.x), (Scene::Height() / CellSize.y) };
	const Rect gridArea{ 0, 0, (gridCount.x * CellSize.x), (gridCount.y * CellSize.y) };

	// フォント登録（UIで使う前に登録）
	FontAsset::Register(U"UI", FontMethod::MSDF, 20, Typeface::Bold);

	// チーム
	enum class Team : int32 { None = 0, Blue = 1, Red = 2 };

	auto TeamColor = [](Team t)->ColorF
		{
			switch (t)
			{
			case Team::Blue: return ColorF{ 0.20, 0.55, 1.0 };
			case Team::Red:  return ColorF{ 1.00, 0.35, 0.35 };
			default:         return ColorF{ 0.90 };
			}
		};

	// グリッドの所有状態
	Grid<Team> owner(gridCount, Team::None);

	// 便利関数
	auto InBounds = [&](const Point& c)->bool
		{
			return (0 <= c.x && c.x < gridCount.x && 0 <= c.y && c.y < gridCount.y);
		};
	auto CellRect = [&](const Point& c)->Rect
		{
			return Rect{ (c.x * CellSize.x), (c.y * CellSize.y), CellSize };
		};
	auto CellCenter = [&](const Point& c)->Vec2
		{
			return Vec2{ (c.x + 0.5) * CellSize.x, (c.y + 0.5) * CellSize.y };
		};
	auto PosToCell = [&](const Vec2& p)->Point
		{
			return Point{
				Clamp<int32>(static_cast<int32>(p.x) / CellSize.x, 0, (gridCount.x - 1)),
				Clamp<int32>(static_cast<int32>(p.y) / CellSize.y, 0, (gridCount.y - 1))
			};
		};

	// 初期のタワー配置（左中段が Blue、右中段が Red）
	const Point blueTowerCell{ Max(2, gridCount.x / 4), gridCount.y / 2 };
	const Point redTowerCell{ Min(gridCount.x - 3, (gridCount.x * 3) / 4), gridCount.y / 2 };

	// 建物
	enum class BType : int32 { Tower, Turret, Facility, Wall, Fortress };

	struct Building
	{
		BType type{};
		Team team{};
		Point cell{};
		// タレット
		double cooldown = 0.0;
		// タワー浸食
		double capture = 0.0; // 進行度
		// 要塞耐久
		int   hp = 0;
		int   maxHP = 0;
		bool  alive = true; // 破壊後は false
	};

	// ゲーム状態
	enum class GameState { Playing, BlueWin, BlueLose };
	GameState gameState = GameState::Playing;

	// 要塞の耐久と弾ダメージ
	constexpr int fortressMaxHP = 200;
	constexpr int fortressHitDamage = 20;

	Array<Building> buildings;
	Grid<int32> buildingAt(gridCount, -1); // -1: なし、>=0: buildings の index

	auto CountAliveFortress = [&](Team team)->int
		{
			int n = 0;
			for (const auto& b : buildings)
			{
				if (b.alive && b.type == BType::Fortress && b.team == team) ++n;
			}
			return n;
		};

	auto PlaceBuilding = [&](BType type, Team team, const Point& cell)->bool
		{
			if (!InBounds(cell)) return false;
			if (buildingAt[cell] != -1) return false;
			// 壁以外は自色タイルの上のみ配置可（壁も今回は自色のみ可に統一）
			if (owner[cell] != team) return false;
			// 要塞は各陣営 1 基まで
			if (type == BType::Fortress && CountAliveFortress(team) >= 1) return false;

			Building b;
			b.type = type;
			b.team = team;
			b.cell = cell;
			b.cooldown = 0.0;
			b.capture = 0.0;
			b.alive = true;
			if (type == BType::Fortress)
			{
				b.maxHP = fortressMaxHP;
				b.hp = fortressMaxHP;
			}

			const int32 idx = static_cast<int32>(buildings.size());
			buildings << Building{ .type = type, .team = team, .cell = cell, .cooldown = 0.0, .capture = 0.0 };
			buildingAt[cell] = idx;
			return true;
		};

	// 初期自陣（左右半分塗り）
	// タワー設置
	PlaceBuilding(BType::Tower, Team::Blue, blueTowerCell);
	PlaceBuilding(BType::Tower, Team::Red, redTowerCell);

	// タワー中心も自色で塗る
	owner[blueTowerCell] = Team::Blue;
	owner[redTowerCell] = Team::Red;

	// 画面を左右で半分ずつ塗る（左=Blue / 右=Red）
	for (int y = 0; y < gridCount.y; ++y)
	{
		for (int x = 0; x < gridCount.x; ++x)
		{
			owner[y][x] = (x < (gridCount.x / 2)) ? Team::Blue : Team::Red;
		}
	}

	// 経済
	double moneyBlue = 100.0;
	double moneyRed = 100.0;
	const double tileIncomePerSec = 0.20;   // 自色タイル収益
	const double facilityIncomePerSec = 3.0; // 施設収益
	double incomeTimer = 0.0;

	// コスト
	const int turretCost = 30;
	const int facilityCost = 50;
	const int wallCost = 10;
	const int fortressCost = 80; // 要塞

	// タレット仕様
	const double turretRange = 220.0;
	const double fireCooldown = 0.6;

	// 浸食仕様
	const double captureTime = 3.0; // これを超えるとタワーが乗っ取られる
	const int captureNeed = 6;      // 8近傍のうち必要な敵色数

	// 弾
	struct Bullet
	{
		Vec2 pos;
		Vec2 vel;
		Point targetCell;
		Team team;
		ColorF color;
	};
	Array<Bullet> bullets;
	constexpr double bulletSpeed = 600.0;
	constexpr double bulletRadius = 6.0;

	// 選択中の建設種別
	BType selected = BType::Turret;

	// AI
	double aiBuildTimer = 0.0;

	// 対象選定：タレットが狙うセル
	auto ChooseTargetForTurret = [&](Team team, const Point& fromCell)->Optional<Point>
		{
			Array<Point> enemy, neutral;
			// 探索半径（セル数）
			const int maxR = static_cast<int>(Ceil(turretRange / static_cast<double>(CellSize.x))) + 1;

			for (int dy = -maxR; dy <= maxR; ++dy)
			{
				for (int dx = -maxR; dx <= maxR; ++dx)
				{
					const Point p = fromCell + Point{ dx, dy };
					if (!InBounds(p)) continue;

					// 射程円判定
					if (CellCenter(fromCell).distanceFrom(CellCenter(p)) > turretRange) continue;

					if (buildingAt[p] != -1 && buildings[buildingAt[p]].type == BType::Wall)
					{
						// 壁セルは狙っても無効なのでスキップ
						continue;
					}

					if (owner[p] == Team::None)
					{
						neutral << p;
					}
					else if (owner[p] != team)
					{
						enemy << p;
					}
				}
			}

			if (!enemy.isEmpty()) return enemy.choice();
			if (!neutral.isEmpty()) return neutral.choice();
			return none;
		};

	// 経済計算（毎秒）
	auto DoIncomeTick = [&]()
		{
			int blueTiles = 0, redTiles = 0;
			int blueFacilities = 0, redFacilities = 0;

			for (int y = 0; y < gridCount.y; ++y)
			{
				for (int x = 0; x < gridCount.x; ++x)
				{
					const Team t = owner[y][x];
					if (t == Team::Blue) ++blueTiles;
					else if (t == Team::Red) ++redTiles;
				}
			}
			for (const auto& b : buildings)
			{
				if (!b.alive) continue;
				if (b.type == BType::Facility)
				{
					if (b.team == Team::Blue) ++blueFacilities;
					else if (b.team == Team::Red) ++redFacilities;
				}
			}

			moneyBlue += blueTiles * tileIncomePerSec + blueFacilities * facilityIncomePerSec;
			moneyRed += redTiles * tileIncomePerSec + redFacilities * facilityIncomePerSec;
		};

	// 入力ヘルパ
	auto IsCellPlaceable = [&](Team team, const Point& cell, BType type)->bool
		{
			if (!InBounds(cell)) return false;
			if (buildingAt[cell] != -1) return false;
			if (owner[cell] != team) return false;
			// 追加制約があればここへ
			return true;
		};

	//========================
	// ここから箱 / トークン機能
	//========================
	constexpr int MaxBoxes = 10;

	// インベントリUI
	const RectF invPanel{ 10, 90, 320, 84 };          // インベントリ表示領域
	const Vec2  genBtnPos{ 20, 180 };                 // 「箱を生成」ボタン位置
	const Vec2  helpPos{ 20, 220 };

	Array<bool> boxes; // 未開封の箱（true/false は使わない。要素が1つ=箱1つ）
	boxes.reserve(MaxBoxes);

	enum class TokenType : int32 { GreenTriangle, YellowCircle, PurpleSquare };
	struct Token
	{
		TokenType type{};
		Vec2 pos{};
		bool dragging{ false };
		Vec2 dragOffset{};
	};
	Array<Token> tokens;

	auto RandomTokenType = []() -> TokenType
		{
			const int r = Random(0, 2);
			if (r == 0) return TokenType::GreenTriangle;
			if (r == 1) return TokenType::YellowCircle;
			return TokenType::PurpleSquare;
		};

	auto TokenColor = [](TokenType t) -> ColorF
		{
			switch (t)
			{
			case TokenType::GreenTriangle: return ColorF{ 0.20, 0.90, 0.35 };
			case TokenType::YellowCircle:  return ColorF{ 1.00, 0.90, 0.25 };
			case TokenType::PurpleSquare:  return ColorF{ 0.70, 0.40, 0.90 };
			}
			return Palette::White;
		};

	// 発動エフェクト（簡易リング）
	struct Burst { Vec2 pos; ColorF col; double time; };
	Array<Burst> bursts;

	// 特性発動（同種3つ以上を重ねた時）
	auto ActivateTrait = [&](TokenType type, const Vec2& center)
		{
			// 可視化
			bursts << Burst{ center, TokenColor(type), 0.35 };

			if (type == TokenType::GreenTriangle)
			{
				// 赤色セルだけを広く Blue に塗り替える
				const Point c = PosToCell(center);

				// セル単位の半径（広さを調整：例 8 → 半径8セル ≒ 320px）
				const int rCells = 8;
				const int r2 = rCells * rCells;

				for (int dy = -rCells; dy <= rCells; ++dy)
				{
					for (int dx = -rCells; dx <= rCells; ++dx)
					{
						if ((dx * dx + dy * dy) > r2) continue; // 円領域

						const Point p = c + Point{ dx, dy };
						if (!InBounds(p)) continue;

						// 壁のあるセルは塗らない（仕様を合わせる）
						const int bi = buildingAt[p];
						if (bi != -1 && buildings[bi].type == BType::Wall)
						{
							continue;
						}

						// 赤セルのみ Blue に変換
						if (owner[p] == Team::Red)
						{
							owner[p] = Team::Blue;
						}
					}
				}
			}
			else if (type == TokenType::YellowCircle)
			{
				// 周囲の弾を消す + 資金ボーナス
				const double radius = 160.0;
				Array<Bullet> alive;
				alive.reserve(bullets.size());
				for (const auto& b : bullets)
				{
					if (b.pos.distanceFrom(center) > radius)
						alive << b;
				}
				bullets.swap(alive);
				moneyBlue += 50.0;
			}
			else if (type == TokenType::PurpleSquare)
			{
				// 中心セルの上下左右に壁を無料設置（置ける場所のみ）
				const Point c = PosToCell(center);
				const Point d4[4] = { {0,-1},{1,0},{0,1},{-1,0} };
				for (const auto& d : d4)
				{
					const Point p = c + d;
					if (IsCellPlaceable(Team::Blue, p, BType::Wall))
					{
						PlaceBuilding(BType::Wall, Team::Blue, p);
					}
				}
			}
		};
	//========================
	// 要塞を初期配置（図形描画用）
	//========================
	{
		const Point blueFortCell = Point{ Max(1, blueTowerCell.x - 3), Max(1, blueTowerCell.y - 2) };
		if (InBounds(blueFortCell) && (buildingAt[blueFortCell] == -1) && (owner[blueFortCell] == Team::Blue))
		{
			PlaceBuilding(BType::Fortress, Team::Blue, blueFortCell);
		}
		const Point redFortCell = Point{ Min(gridCount.x - 2, redTowerCell.x + 3), Max(1, redTowerCell.y - 2) };
		if (InBounds(redFortCell) && (buildingAt[redFortCell] == -1) && (owner[redFortCell] == Team::Red))
		{
			PlaceBuilding(BType::Fortress, Team::Red, redFortCell);
		}
	}

	// UI 補助
	auto DrawHPBar = [&](const Rect& r, int hp, int maxHP)
		{
			const double ratio = (maxHP > 0 ? Clamp<double>(static_cast<double>(hp) / maxHP, 0.0, 1.0) : 0.0);
			const RectF barBG{ r.x + 4, r.y - 10, r.w - 8, 6 };
			const RectF barFG{ barBG.x, barBG.y, (barBG.w * ratio), barBG.h };
			barBG.draw(ColorF{ 0,0,0,0.35 });
			barFG.draw(ColorF{ 0.2, 0.9, 0.3, 0.9 });
			barBG.drawFrame(1, ColorF{ 0,0,0,0.6 });
		};

	while (System::Update())
	{
		const double dt = Scene::DeltaTime();

		// 入力（建設選択）— 勝敗決着後は無効
		if (gameState == GameState::Playing)
		{
			if (Key1.down()) selected = BType::Turret;
			if (Key2.down()) selected = BType::Facility;
			if (Key3.down()) selected = BType::Wall;
			if (Key4.down()) selected = BType::Fortress; // 要塞（各陣営 1 基まで）
		}

		// ホバーセル
		const Point hoveredCell{
			Clamp<int32>(Cursor::Pos().x / CellSize.x, 0, (gridCount.x - 1)),
			Clamp<int32>(Cursor::Pos().y / CellSize.y, 0, (gridCount.y - 1))
		};

		// プレイヤー配置（建物）
		if (gameState == GameState::Playing && MouseL.down() && gridArea.mouseOver())
		{
			int cost = 0;
			switch (selected)
			{
			case BType::Turret:   cost = turretCost;   break;
			case BType::Facility: cost = facilityCost; break;
			case BType::Wall:     cost = wallCost;     break;
			case BType::Fortress: cost = fortressCost; break;
			default: break;
			}

			if (moneyBlue >= cost && IsCellPlaceable(Team::Blue, hoveredCell, selected))
			{
				if (PlaceBuilding(selected, Team::Blue, hoveredCell))
				{
					moneyBlue -= cost;
				}
			}
		}

		// タレットの自動射撃 — 勝敗決着後は停止
		if (gameState == GameState::Playing)
		{
			for (auto& b : buildings)
			{
				if (!b.alive) continue;
				if (b.type != BType::Turret) continue;

				b.cooldown -= dt;
				if (b.cooldown <= 0.0)
				{
					const Optional<Point> tgt = ChooseTargetForTurret(b.team, b.cell);
					if (tgt)
					{
						const Vec2 from = CellCenter(b.cell);
						const Vec2 to = CellCenter(tgt.value());
						const Vec2 vel = (to - from).withLength(bulletSpeed);

						bullets << Bullet{
							.pos = from,
							.vel = vel,
							.targetCell = tgt.value(),
							.team = b.team,
							.color = TeamColor(b.team)
						};
						b.cooldown = fireCooldown;
					}
					else
					{
						// 狙えるものが無ければ少し待つ
						b.cooldown = 0.25;
					}
				}
			}
		}

		// 弾更新・着弾処理 — 勝敗決着後は停止
		if (gameState == GameState::Playing)
		{
			Array<Bullet> aliveBullets;
			aliveBullets.reserve(bullets.size());

			for (auto& bl : bullets)
			{
				bl.pos += (bl.vel * dt);

				const Vec2 targetPos = CellCenter(bl.targetCell);
				if (bl.pos.distanceFrom(targetPos) <= bulletRadius)
				{
					// 建物チェック
					const int32 bi = buildingAt[bl.targetCell];
					if (bi != -1)
					{
						Building& bd = buildings[bi];
						if (!bd.alive)
						{
							// 既に破壊済みなら無視して塗りに進む
						}
						else if (bd.type == BType::Wall)
						{
							// 壁で無効化（塗れない・ダメージなし）
							continue;
						}
						else if (bd.type == BType::Fortress)
						{
							// 要塞にダメージ（敵弾のみ）
							if (bd.team != bl.team)
							{
								bd.hp -= fortressHitDamage;
								// ヒットエフェクト（簡易）
								Circle{ targetPos, 10 }.draw(ColorF{ 1,0.2,0.2,0.25 });
								if (bd.hp <= 0)
								{
									bd.hp = 0;
									bd.alive = false;
									buildingAt[bd.cell] = -1;

									// 勝敗判定
									if (bd.team == Team::Blue)
									{
										gameState = GameState::BlueLose; // プレイヤー要塞破壊 → Game Over
									}
									else if (bd.team == Team::Red)
									{
										gameState = GameState::BlueWin;  // 敵要塞破壊 → Clear
									}
								}
							}
							// 命中した弾は消える（塗りはしない）
							continue;
						}
					}

					// 建物に阻まれなかった場合はセルを塗る
					owner[bl.targetCell] = bl.team;
					continue;
				}

				aliveBullets << bl;
			}

			bullets.swap(aliveBullets);
		}

		// タワー浸食処理 — 勝敗決着後も一旦停止（今回は勝敗に関与させない）
		if (gameState == GameState::Playing)
		{
			for (auto& b : buildings)
			{
				if (!b.alive) continue;
				if (b.type != BType::Tower) continue;

				int enemyCount = 0;
				const Team enemyTeam = (b.team == Team::Blue ? Team::Red : Team::Blue);

				for (int dy = -1; dy <= 1; ++dy)
				{
					for (int dx = -1; dx <= 1; ++dx)
					{
						if (dx == 0 && dy == 0) continue;
						const Point p = b.cell + Point{ dx, dy };
						if (!InBounds(p)) continue;
						if (owner[p] == enemyTeam) ++enemyCount;
					}
				}

				if (enemyCount >= captureNeed)
				{
					b.capture = Min(captureTime, b.capture + dt);
				}
				else
				{
					b.capture = Max(0.0, b.capture - dt * 0.5);
				}

				if (b.capture >= captureTime)
				{
					// タワーの所有権を移す（今回は勝敗に影響しない）
					b.team = (b.team == Team::Blue ? Team::Red : Team::Blue);
					owner[b.cell] = b.team;
					b.capture = 0.0;
				}
			}
		}

		// 経済（毎秒）— 勝敗決着後は停止
		if (gameState == GameState::Playing)
		{
			incomeTimer += dt;
			while (incomeTimer >= 1.0)
			{
				incomeTimer -= 1.0;
				DoIncomeTick();
			}
		}

		// AI（簡易）：定期的に建設 — 勝敗決着後は停止
		if (gameState == GameState::Playing)
		{
			aiBuildTimer += dt;
			if (aiBuildTimer >= 2.0)
			{
				aiBuildTimer = 0.0;

				// 建てるものを選ぶ（資金に応じて）
				struct Choice { BType t; int cost; double w; };
				Array<Choice> pool;

				if (moneyRed >= turretCost)   pool << Choice{ BType::Turret,   turretCost,   0.5 };
				if (moneyRed >= facilityCost) pool << Choice{ BType::Facility, facilityCost, 0.35 };
				if (moneyRed >= wallCost)     pool << Choice{ BType::Wall,     wallCost,     0.15 };

				if (!pool.isEmpty())
				{
					// 重み選択
					const double totalW = pool.map([](const Choice& c) { return c.w; }).sum();
					double r = Random(0.0, totalW);
					BType pick = pool.back().t;
					int cost = pool.back().cost;
					for (const auto& c : pool)
					{
						if ((r -= c.w) <= 0.0) { pick = c.t; cost = c.cost; break; }
					}

					// 自陣の空きマスからランダム
					Array<Point> candidates;
					for (int y = 0; y < gridCount.y; ++y)
					{
						for (int x = 0; x < gridCount.x; ++x)
						{
							const Point p{ x, y };
							// Red 陣営の条件
							const bool placeable = (InBounds(p) && buildingAt[p] == -1 && owner[p] == Team::Red);
							if (placeable)
							{
								candidates << p;
							}
						}
					}
					if (!candidates.isEmpty())
					{
						const Point c = candidates.choice();
						if (PlaceBuilding(pick, Team::Red, c))
						{
							moneyRed -= cost;
						}
					}
				}
			}
		}

		//========================
		// 描画
		//========================

		// 描画: グリッド背景
		gridArea.draw(ColorF{ 0.97 });

		// タイルの色
		for (int y = 0; y < gridCount.y; ++y)
		{
			for (int x = 0; x < gridCount.x; ++x)
			{
				const Team t = owner[y][x];
				if (t == Team::None) continue;

				ColorF c = TeamColor(t);
				c.a = 0.80;
				CellRect(Point{ x, y }).stretched(-1).draw(c);
			}
		}

		// グリッド線
		for (int y = 0; y <= gridCount.y; ++y)
		{
			const int ypx = (y * CellSize.y);
			Line{ 0, ypx, gridArea.w, ypx }.draw(ColorF{ 0.0, 0.0, 0.0, 0.14 });
		}
		for (int x = 0; x <= gridCount.x; ++x)
		{
			const int xpx = (x * CellSize.x);
			Line{ xpx, 0, xpx, gridArea.h }.draw(ColorF{ 0.0, 0.0, 0.0, 0.14 });
		}

		// 建物の描画
		for (const auto& b : buildings)
		{
			if (!b.alive) continue;

			const Rect r = CellRect(b.cell).stretched(-3);
			const ColorF base = TeamColor(b.team);

			if (b.type == BType::Tower)
			{
				r.stretched(-2).rounded(6).draw(base);
				r.rounded(6).drawFrame(3, ColorF{ 0,0,0,0.5 });
				// 浸食ゲージ
				if (b.capture > 0.0)
				{
					const double ratio = (b.capture / captureTime);
					const double rad = (Min(r.w, r.h) * 0.5 - 6);
					Circle{ r.center(), rad }.drawArc(-90_deg, (360_deg * ratio), 4, 4, ColorF{ 1,1,1,0.9 });
				}
			}
			else if (b.type == BType::Turret)
			{
				r.stretched(-4).rounded(4).draw(base);
				r.drawFrame(2, ColorF{ 0,0,0,0.45 });
			}
			else if (b.type == BType::Facility)
			{
				const Rect rf = r.stretched(-6);
				Triangle{ rf.center(), rf.w / 2.0, 0_deg }.draw(base);
				r.drawFrame(2, ColorF{ 0,0,0,0.45 });
			}
			else if (b.type == BType::Wall)
			{
				r.draw(ColorF{ 0.15,0.15,0.15 });
				r.drawFrame(2, base);
			}
			else if (b.type == BType::Fortress)
			{
				const Vec2  c = r.center();
				// 外壁リング
				const double ringR = Min(r.w, r.h) * 0.5 - 4;
				Circle{ c, ringR }.drawFrame(4, base);
				Circle{ c, ringR - 6 }.drawFrame(2, ColorF{ 0,0,0,0.35 });
				// 天守（中心の丸み四角）
				const RectF keep = r.stretched(-10);
				keep.rounded(5).draw(base);
				keep.rounded(5).drawFrame(2, ColorF{ 0,0,0,0.45 });
				// 四隅の小塔（円）
				const double tr = 5.0;
				Circle{ r.tl().movedBy(10, 10), tr }.draw(base);
				Circle{ r.tr().movedBy(-10, 10), tr }.draw(base);
				Circle{ r.bl().movedBy(10, -10), tr }.draw(base);
				Circle{ r.br().movedBy(-10, -10), tr }.draw(base);
				// 門（下側）
				RectF{ c.x - 5, r.y + r.h - 10, 10, 8 }.draw(ColorF{ 0,0,0,0.6 });
				// 旗（上部）
				Triangle{ r.topCenter().movedBy(0, 6), 9, 0_deg }.draw(base);
				Line{ r.topCenter().movedBy(0, 6), r.topCenter().movedBy(0, 20) }.draw(2, base);
				// HPバー
				DrawHPBar(r, b.hp, b.maxHP);
			}
		}

		// 弾を描画
		for (const auto& bl : bullets)
		{
			Circle{ bl.pos, bulletRadius }.draw(bl.color);
			Line{ bl.pos, (bl.pos - bl.vel.withLength(24.0)) }.draw(4, bl.color.withAlpha(0.45));
		}

		// トークン描画（クリップ）
		{
			//const ScopedScissorRect2D scissor{ gridArea };
			for (const auto& t : tokens)
			{
				const ColorF col = TokenColor(t.type);
				if (t.type == TokenType::GreenTriangle)
				{
					Triangle{ t.pos, 16, 0_deg }.draw(col);
				}
				else if (t.type == TokenType::YellowCircle)
				{
					Circle{ t.pos, 10 }.draw(col);
					Circle{ t.pos, 10 }.drawFrame(2, ColorF{ 0,0,0,0.35 });
				}
				else if (t.type == TokenType::PurpleSquare)
				{
					RectF{ t.pos.x - 10, t.pos.y - 10, 20, 20 }.rounded(4).draw(col);
				}
			}
		}

		// ホバー中セルのハイライトとタレット射程プレビュー
		if (gameState == GameState::Playing)
		{
			const Rect r = CellRect(hoveredCell).stretched(-2);
			const bool can =
				(InBounds(hoveredCell)
				 && buildingAt[hoveredCell] == -1
				 && owner[hoveredCell] == Team::Blue
				 && (selected != BType::Fortress || CountAliveFortress(Team::Blue) < 1));
			if (can) r.draw(ColorF{ 0.2, 0.8, 0.3, 0.20 });
			r.drawFrame(2, can ? ColorF{ 0.2, 0.8, 0.3, 0.9 } : ColorF{ 0.8, 0.2, 0.2, 0.6 });

			if (selected == BType::Turret && can)
			{
				Circle{ CellCenter(hoveredCell), turretRange }.drawFrame(2, ColorF{ 0.2,0.6,1.0,0.45 });
			}
		}

		// エフェクト描画
		for (auto& b : bursts)
		{
			const double a = Saturate(b.time / 0.35);
			const double r = 18 + 80 * (1.0 - a);
			Circle{ b.pos, r }.drawFrame(3, b.col.withAlpha(0.45 * a));
			b.time -= dt;
		}
		bursts.remove_if([](const Burst& b) { return b.time <= 0.0; });
		//========================
		// 箱インベントリ UI と開封 -> トークン生成
		//========================
		// パネルとスロット描画
		{
			// インベントリパネル
			invPanel.draw(ColorF{ 1,1,1,0.85 });
			invPanel.drawFrame(2, ColorF{ 0.35,0.25,0.18,0.9 });

			// スロットを描画
			const int slotCount = MaxBoxes;
			const double slotW = 24, slotH = 24, slotPad = 4;
			const double startX = invPanel.x + 10;
			const double startY = invPanel.y + 10;

			for (int i = 0; i < slotCount; ++i)
			{
				const double x = startX + i * (slotW + slotPad);
				const RectF slot{ x, startY, slotW, slotH };
				slot.draw(ColorF{ 0.95 });
				slot.drawFrame(1, ColorF{ 0,0,0,0.25 });

				if (i < boxes.size())
				{
					const bool hover = slot.mouseOver();
					const ColorF brown{ 0.50, 0.35, 0.18, hover ? 1.0 : 0.95 };
					slot.stretched(-2).rounded(4).draw(brown);
					slot.rounded(4).drawFrame(2, ColorF{ 0,0,0,0.35 });

					// 箱をクリックで開封（ここで tokens に追加、boxes を減らす）
					if (hover && MouseL.down())
					{
						const Vec2 spawn = CellCenter(hoveredCell);
						tokens << Token{ .type = RandomTokenType(), .pos = spawn, .dragging = false, .dragOffset = Vec2{ 0,0 } };
						boxes.erase(boxes.begin() + i);
						// 1フレームで複数開封しないよう break
						break;
					}
				}
			}

			// 箱を生成ボタン（最大10個）
			if (SimpleGUI::Button(U"箱を生成", genBtnPos, 160))
			{
				if (static_cast<int>(boxes.size()) < MaxBoxes)
				{
					boxes << false; // 1つ追加
				}
			}
			FontAsset(U"UI")(U"箱: {}/10"_fmt(boxes.size())).draw(20, genBtnPos.movedBy(170, 4), Palette::Black);
			FontAsset(U"UI")(U"箱をクリックで開封 → トークン生成（ドラッグで移動）")
				.draw(18, helpPos, ColorF{ 0,0,0,0.8 });
		}


		//========================
		// トークンのドラッグ移動
		//========================
		// 先にドラッグ開始判定
		if (MouseL.down())
		{
			for (int i = static_cast<int>(tokens.size()) - 1; i >= 0; --i)
			{
				const Vec2 p = tokens[i].pos;
				// 当たり半径（見た目より少し大きめ）
				if (p.distanceFrom(Cursor::PosF()) <= 18.0)
				{
					tokens[i].dragging = true;
					tokens[i].dragOffset = (tokens[i].pos - Cursor::PosF());
					break;
				}
			}
		}
		if (MouseL.pressed())
		{
			for (auto& t : tokens)
			{
				if (!t.dragging) continue;
				Vec2 np = Cursor::PosF() + t.dragOffset;
				// グリッド内にクランプ
				np.x = Clamp<double>(np.x, gridArea.x + 2, gridArea.x + gridArea.w - 2);
				np.y = Clamp<double>(np.y, gridArea.y + 2, gridArea.y + gridArea.h - 2);
				t.pos = np;
			}
		}
		if (MouseL.up())
		{
			for (auto& t : tokens)
			{
				if (t.dragging)
				{
					t.dragging = false;
					// セル中心にスナップ
					t.pos = CellCenter(PosToCell(t.pos));
				}
			}
		}

		//========================
		// トークンの「重ね」検出と特性発動
		//========================
		const double stackRadius = 14.0; // 同種でこの距離以内を「重ね」とみなす
		Array<int> toRemove; toRemove.reserve(tokens.size());
		// 種別ごとに判定
		for (TokenType ty : { TokenType::GreenTriangle, TokenType::YellowCircle, TokenType::PurpleSquare })
		{
			for (int i = 0; i < tokens.size(); ++i)
			{
				if (tokens[i].type != ty) continue;
				// 既に消去予定ならスキップ
				if (toRemove.includes(i)) continue;

				// i を中心にグループ化
				Array<int> group{ i };
				for (int j = 0; j < tokens.size(); ++j)
				{
					if (i == j) continue;
					if (tokens[j].type != ty) continue;
					if (toRemove.includes(j)) continue;
					if (tokens[i].pos.distanceFrom(tokens[j].pos) <= stackRadius)
					{
						group << j;
					}
				}

				if (group.size() >= 3)
				{
					// 中心座標（平均）で特性発動
					Vec2 center{ 0,0 };
					for (int gi : group) center += tokens[gi].pos;
					center /= static_cast<double>(group.size());

					ActivateTrait(ty, center);

					// グループのトークンを消費マーク
					for (int gi : group) toRemove << gi;
				}
			}
		}
		if (!toRemove.isEmpty())
		{
			toRemove.sort_by([](int a, int b) { return a > b; });
			for (int idx : toRemove)
			{
				if (0 <= idx && idx < tokens.size()) tokens.erase(tokens.begin() + idx);
			}
		}


		// UI
		const String selName =
			(selected == BType::Turret ? U"タレット(1)" :
			 selected == BType::Facility ? U"施設(2)" :
			 selected == BType::Wall ? U"壁(3)" :
			 selected == BType::Fortress ? U"要塞(4)" : U"");

		const int selCost =
			(selected == BType::Turret ? turretCost :
			 selected == BType::Facility ? facilityCost :
			 selected == BType::Wall ? wallCost :
			 selected == BType::Fortress ? fortressCost : 0);

		// プレイヤー情報
		{
			const RectF pbox{ 10, 8, 320, 68 };
			pbox.draw(ColorF{ 1,1,1,0.85 });
			pbox.drawFrame(2, TeamColor(Team::Blue));
			FontAsset(U"UI")(U"Blue 資金: {:.1f}"_fmt(moneyBlue)).draw(24, Vec2{ 20, 12 }, Palette::Black);
			FontAsset(U"UI")(U"選択: {} / コスト: {}"_fmt(selName, selCost)).draw(20, Vec2{ 20, 42 }, Palette::Black);
		}
		// 敵情報
		{
			const RectF ebox{ Scene::Width() - 330, 8, 320, 68 };
			ebox.draw(ColorF{ 1,1,1,0.85 });
			ebox.drawFrame(2, TeamColor(Team::Red));
			FontAsset(U"UI")(U"Red 資金: {:.1f}"_fmt(moneyRed)).draw(24, Vec2{ Scene::Width() - 320, 12 }, Palette::Black);
			FontAsset(U"UI")(U"AI: 建設中...").draw(20, Vec2{ Scene::Width() - 320, 42 }, Palette::Black);
		}

		// 勝敗オーバーレイ
		if (gameState != GameState::Playing)
		{
			RectF{ 0,0, Scene::Width(), Scene::Height() }.draw(ColorF{ 0,0,0,0.55 });
			String text = (gameState == GameState::BlueWin) ? U"CLEAR!" : U"GAME OVER";
			const ColorF col = (gameState == GameState::BlueWin) ? ColorF{ 0.2,1.0,0.4 } : ColorF{ 1.0,0.3,0.3 };
			FontAsset(U"UI")(text).drawAt(72, Scene::Center().movedBy(0, -20), col);
			FontAsset(U"UI")(U"[R] でリスタート").drawAt(28, Scene::Center().movedBy(0, 40), Palette::White);

			if (KeyR.down())
			{
				// いったんアプリ再起動相当の簡易リセット（プロジェクトに合わせて正式リセット処理に置き換え可能）
				System::Exit(); // エディタから再実行 or アプリを再起動してください
			}
		}
	}
}
