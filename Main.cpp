# include <Siv3D.hpp> // Siv3D v0.6.16

void Main()
{
	// 基本設定
	Scene::SetBackground(ColorF{ 0.98 });
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

	// 初期のタワー配置（左中段が Blue、右中段が Red）
	const Point blueTowerCell{ Max(2, gridCount.x / 4), gridCount.y / 2 };
	const Point redTowerCell{ Min(gridCount.x - 3, (gridCount.x * 3) / 4), gridCount.y / 2 };

	// 建物
	enum class BType : int32 { Tower, Turret, Facility, Wall };

	struct Building
	{
		BType type{};
		Team team{};
		Point cell{};
		// タレット
		double cooldown = 0.0;
		// タワー浸食
		double capture = 0.0; // 進行度
	};

	Array<Building> buildings;
	Grid<int32> buildingAt(gridCount, -1); // -1: なし、>=0: buildings の index

	auto PlaceBuilding = [&](BType type, Team team, const Point& cell)->bool
		{
			if (!InBounds(cell)) return false;
			if (buildingAt[cell] != -1) return false;
			// 壁以外は自色タイルの上のみ配置可（壁も今回は自色のみ可に統一）
			if (owner[cell] != team) return false;

			const int32 idx = static_cast<int32>(buildings.size());
			buildings << Building{ .type = type, .team = team, .cell = cell, .cooldown = 0.0, .capture = 0.0 };
			buildingAt[cell] = idx;
			return true;
		};

	// 初期自陣（タワー周辺を少し塗る）
	auto PaintDisk = [&](const Point& center, int r, Team team)
		{
			for (int dy = -r; dy <= r; ++dy)
			{
				for (int dx = -r; dx <= r; ++dx)
				{
					const Point p = center + Point{ dx, dy };
					if (!InBounds(p)) continue;
					if ((dx * dx + dy * dy) <= (r * r))
					{
						owner[p] = team;
					}
				}
			}
		};

	// タワー設置
	PlaceBuilding(BType::Tower, Team::Blue, blueTowerCell);
	PlaceBuilding(BType::Tower, Team::Red, redTowerCell);

	// タワー中心も自色で塗る
	owner[blueTowerCell] = Team::Blue;
	owner[redTowerCell] = Team::Red;

	// 初期領域
	PaintDisk(blueTowerCell, 3, Team::Blue);
	PaintDisk(redTowerCell, 3, Team::Red);

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

	while (System::Update())
	{
		const double dt = Scene::DeltaTime();

		// 入力（建設選択）
		if (Key1.down()) selected = BType::Turret;
		if (Key2.down()) selected = BType::Facility;
		if (Key3.down()) selected = BType::Wall;

		// ホバーセル
		const Point hoveredCell{
			Clamp<int32>(Cursor::Pos().x / CellSize.x, 0, (gridCount.x - 1)),
			Clamp<int32>(Cursor::Pos().y / CellSize.y, 0, (gridCount.y - 1))
		};

		// プレイヤー配置
		if (MouseL.down() && gridArea.mouseOver())
		{
			int cost = 0;
			switch (selected)
			{
			case BType::Turret:   cost = turretCost;   break;
			case BType::Facility: cost = facilityCost; break;
			case BType::Wall:     cost = wallCost;     break;
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

		// タレットの自動射撃
		for (auto& b : buildings)
		{
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

		// 弾更新・着弾処理
		{
			Array<Bullet> alive;
			alive.reserve(bullets.size());

			for (auto& bl : bullets)
			{
				bl.pos += (bl.vel * dt);

				const Vec2 targetPos = CellCenter(bl.targetCell);
				if (bl.pos.distanceFrom(targetPos) <= bulletRadius)
				{
					// 壁チェック
					const int32 bi = buildingAt[bl.targetCell];
					if (bi != -1 && buildings[bi].type == BType::Wall)
					{
						// 壁で無効化（塗れない）
						continue;
					}

					// セルを塗る
					owner[bl.targetCell] = bl.team;
					continue;
				}

				alive << bl;
			}

			bullets.swap(alive);
		}

		// タワー浸食処理
		for (auto& b : buildings)
		{
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
				// 視覚フィードバックは描画側で
			}
			else
			{
				b.capture = Max(0.0, b.capture - dt * 0.5);
			}

			if (b.capture >= captureTime)
			{
				// タワーの所有権を移す
				b.team = enemyTeam;
				owner[b.cell] = enemyTeam;
				b.capture = 0.0;
			}
		}

		// 経済（毎秒）
		incomeTimer += dt;
		while (incomeTimer >= 1.0)
		{
			incomeTimer -= 1.0;
			DoIncomeTick();
		}

		// AI（簡易）：定期的に建設
		aiBuildTimer += dt;
		if (aiBuildTimer >= 2.0)
		{
			aiBuildTimer = 0.0;

			// 建てるものを選ぶ（資金に応じて）
			struct Choice { BType t; int cost; double w; };
			Array<Choice> pool;

			if (moneyRed >= turretCost)   pool << Choice{ BType::Turret, turretCost,   0.5 };
			if (moneyRed >= facilityCost) pool << Choice{ BType::Facility, facilityCost, 0.35 };
			if (moneyRed >= wallCost)     pool << Choice{ BType::Wall, wallCost,     0.15 };

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
						if (IsCellPlaceable(Team::Red, p, pick))
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
			const Rect r = CellRect(b.cell).stretched(-3);
			const ColorF base = TeamColor(b.team);

			if (b.type == BType::Tower)
			{
				r.stretched(-2).rounded(6).draw(base);
				r.rounded(6).drawFrame(3, ColorF{ 0,0,0,0.5 });
				// 浸食ゲージ（RectではなくCircleのdrawArcを使用）
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
		}

		// 弾を描画
		for (const auto& bl : bullets)
		{
			Circle{ bl.pos, bulletRadius }.draw(bl.color);
			Line{ bl.pos, (bl.pos - bl.vel.withLength(24.0)) }.draw(4, bl.color.withAlpha(0.45));
		}

		// ホバー中セルのハイライトとタレット射程プレビュー
		{
			const Rect r = CellRect(hoveredCell).stretched(-2);
			const bool can = IsCellPlaceable(Team::Blue, hoveredCell, selected);
			if (can) r.draw(ColorF{ 0.2, 0.8, 0.3, 0.20 });
			r.drawFrame(2, can ? ColorF{ 0.2, 0.8, 0.3, 0.9 } : ColorF{ 0.8, 0.2, 0.2, 0.6 });

			if (selected == BType::Turret && can)
			{
				Circle{ CellCenter(hoveredCell), turretRange }.drawFrame(2, ColorF{ 0.2,0.6,1.0,0.45 });
			}
		}

		// UI
		const String selName =
			(selected == BType::Turret ? U"タレット(1)" :
				selected == BType::Facility ? U"施設(2)" :
				selected == BType::Wall ? U"壁(3)" : U"");

		const int selCost =
			(selected == BType::Turret ? turretCost :
			 selected == BType::Facility ? facilityCost :
			 selected == BType::Wall ? wallCost : 0);

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
	}
}

// 実験データ: 塗り弾を撃つだけのデモ
// {

	//// シーン背景
	//Scene::SetBackground(ColorF{ 0.98 });

	//// グリッド設定
	//constexpr Size CellSize{ 40, 40 };
	//const Size gridCount{ (Scene::Width() / CellSize.x), (Scene::Height() / CellSize.y) };
	//const Rect gridArea{ 0, 0, (gridCount.x * CellSize.x), (gridCount.y * CellSize.y) };

	//// タレット（グリッド中央のセル）
	//const Point turretCell{ (gridCount.x / 2), (gridCount.y / 2) };
	//const Vec2 turretPos{ (turretCell.x * CellSize.x + CellSize.x * 0.5), (turretCell.y * CellSize.y + CellSize.y * 0.5) };

	//// 選択範囲（半径）
	//const double maxRange = 220.0;

	//// 弾設定
	//constexpr double bulletSpeed = 600.0;   // px/sec
	//constexpr double bulletRadius = 6.0;

	//// セル塗り状態（未塗り: none / 塗り: 色）
	//Grid<Optional<ColorF>> painted{ gridCount };

	//// 弾のデータ
	//struct Bullet
	//{
	//	Vec2 pos;
	//	Vec2 vel;
	//	Vec2 targetPos;
	//	Point targetCell;
	//	ColorF color;
	//};

	//Array<Bullet> bullets;

	//// 補助関数
	//auto CellRect = [&](const Point& cell) -> Rect
	//	{
	//		return Rect{ (cell.x * CellSize.x), (cell.y * CellSize.y), CellSize };
	//	};

	//auto CellCenter = [&](const Point& cell) -> Vec2
	//	{
	//		return Vec2{ (cell.x * CellSize.x + CellSize.x * 0.5), (cell.y * CellSize.y + CellSize.y * 0.5) };
	//	};

	//while (System::Update())
	//{
	//	const double dt = Scene::DeltaTime();

	//	// マウスのセル座標
	//	Point hoveredCell{
	//		Clamp<int32>(Cursor::Pos().x / CellSize.x, 0, (gridCount.x - 1)),
	//		Clamp<int32>(Cursor::Pos().y / CellSize.y, 0, (gridCount.y - 1))
	//	};
	//	const Vec2 hoveredCenter = CellCenter(hoveredCell);
	//	const bool hoveredInRange = (turretPos.distanceFrom(hoveredCenter) <= maxRange);

	//	// クリックで射撃（範囲内のみ）
	//	if (MouseL.down() && gridArea.mouseOver() && hoveredInRange)
	//	{
	//		const Vec2 targetPos = hoveredCenter;

	//		// 着弾時の塗り色（位置ベースで決定）
	//		const double hue = Fmod(static_cast<double>(hoveredCell.y * 30 + hoveredCell.x * 13), 360.0);
	//		const ColorF color = HSV{ hue, 0.9, 1.0 };

	//		const Vec2 vel = (targetPos - turretPos).withLength(bulletSpeed);

	//		bullets << Bullet{
	//			.pos = turretPos,
	//			.vel = vel,
	//			.targetPos = targetPos,
	//			.targetCell = hoveredCell,
	//			.color = color
	//		};
	//	}

	//	// 弾の更新と着弾処理
	//	{
	//		Array<Bullet> alive;
	//		alive.reserve(bullets.size());

	//		for (auto& b : bullets)
	//		{
	//			b.pos += (b.vel * dt);

	//			// 着弾判定（ターゲット中心との距離で判定）
	//			if (b.pos.distanceFrom(b.targetPos) <= bulletRadius)
	//			{
	//				painted[b.targetCell.y][b.targetCell.x] = b.color; // セルを塗る
	//			}
	//			else
	//			{
	//				alive << b;
	//			}
	//		}

	//		bullets.swap(alive);
	//	}

	//	// グリッドの下地
	//	gridArea.draw(ColorF{ 0.97 });

	//	// 塗られたセルを描画
	//	for (int y = 0; y < gridCount.y; ++y)
	//	{
	//		for (int x = 0; x < gridCount.x; ++x)
	//		{
	//			if (painted[y][x])
	//			{
	//				CellRect(Point{ x, y }).stretched(-1).draw(painted[y][x].value());
	//			}
	//		}
	//	}

	//	// グリッド線
	//	for (int y = 0; y <= gridCount.y; ++y)
	//	{
	//		const int ypx = (y * CellSize.y);
	//		Line{ 0, ypx, gridArea.w, ypx }.draw(ColorF{ 0.0, 0.0, 0.0, 0.14 });
	//	}
	//	for (int x = 0; x <= gridCount.x; ++x)
	//	{
	//		const int xpx = (x * CellSize.x);
	//		Line{ xpx, 0, xpx, gridArea.h }.draw(ColorF{ 0.0, 0.0, 0.0, 0.14 });
	//	}

	//	// ホバー中セルのハイライト
	//	{
	//		const Rect r = CellRect(hoveredCell).stretched(-2);
	//		if (hoveredInRange)
	//		{
	//			r.draw(ColorF{ 0.2, 0.65, 1.0, 0.16 });
	//			r.drawFrame(2, ColorF{ 0.2, 0.65, 1.0, 0.85 });
	//		}
	//		else
	//		{
	//			r.drawFrame(2, ColorF{ 0.5, 0.5, 0.5, 0.35 });
	//		}
	//	}

	//	// タレットの選択範囲（円）
	//	Circle{ turretPos, maxRange }.drawFrame(3, ColorF{ 0.25, 0.6, 1.0, 0.55 });
	//	// ほんのりパルス
	//	{
	//		const double pulse = 8.0 * Abs(Sin(Scene::Time() * 1.2));
	//		Circle{ turretPos, (maxRange - 12.0 + pulse) }.drawFrame(2, ColorF{ 0.25, 0.6, 1.0, 0.28 });
	//	}

	//	// タレット本体（中央の四角）
	//	{
	//		const RectF turretRect{
	//			(turretPos.x - CellSize.x * 0.3),
	//			(turretPos.y - CellSize.y * 0.3),
	//			(CellSize.x * 0.6), (CellSize.y * 0.6)
	//		};
	//		turretRect.rounded(4).draw(Palette::Orange);
	//		turretRect.drawFrame(2, ColorF{ 0.1, 0.1, 0.1, 0.65 });
	//	}

	//	// 弾を描画
	//	for (const auto& b : bullets)
	//	{
	//		Circle{ b.pos, bulletRadius }.draw(b.color);
	//		// 射線の薄いトレイル
	//		Line{ b.pos, (b.pos - b.vel.withLength(24.0)) }.draw(4, b.color.withAlpha(0.45));
	//	}
	//}
// }
