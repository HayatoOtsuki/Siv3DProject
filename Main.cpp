# include <Siv3D.hpp> // Siv3D v0.6.16
#include "Game.h"

enum class AppState { Title, Playing };

// Rom/ または rom/ 配下から音を探してロード
static s3d::Audio LoadAudioFromRom(const s3d::FilePath& relUnderRom) {
	using namespace s3d;
	const Array<FilePath> candidates = {
		U"Rom/" + relUnderRom,
		U"rom/" + relUnderRom,
		U"../Rom/" + relUnderRom,
		U"../rom/" + relUnderRom,
		U"../../Rom/" + relUnderRom,
		U"../../rom/" + relUnderRom,
	};
	for (const auto& p : candidates) {
		if (FileSystem::Exists(p)) {
			Audio a{ p };
			if (a) return a;
		}
	}
	return {};
}

void Main() {
	using namespace s3d;

	Window::Resize(1600, 900);
	Scene::SetBackground(ColorF{ 0.06, 0.06, 0.07 });
	FontAsset::Register(U"UI", FontMethod::MSDF, 20, Typeface::Bold);
	const ScopedRenderStates2D _sampler{ SamplerState::ClampLinear };

	// タイトル/ゲームBGMとUIボタン音
	Audio bgmTitle = LoadAudioFromRom(U"audio/Title_BGM.mp3");
	Audio bgmGame = LoadAudioFromRom(U"audio/GameBGM.mp3");
	Audio uiButton = LoadAudioFromRom(U"audio/Buttom.mp3"); // ファイル名はButtom.mp3想定

	// ループ設定
	if (bgmTitle) bgmTitle.setLoop(true);
	if (bgmGame)  bgmGame.setLoop(true);

	Game G;
	AppState state = AppState::Title;
	bool gameInitialized = false;

	while (System::Update()) {
		const double dtReal = Scene::DeltaTime();

		// タイトル画面
		if (state == AppState::Title) {
			// タイトルBGM再生（未再生なら）
			if (bgmTitle && !bgmTitle.isPlaying()) {
				bgmGame.stop();
				bgmTitle.setLoop(true);
				bgmTitle.setVolume(1.0);   // 任意（既定1.0）
				bgmTitle.play();           // 引数なしで再生
			}

			// 背景とタイトル
			const RectF panel{ 0, 0, (double)Scene::Width(), (double)Scene::Height() };
			panel.draw(ColorF{ 0,0,0,0.15 });

			FontAsset(U"UI")(U"浸食！インクウォーズ").drawAt(48, Scene::Center().movedBy(0, -140), ColorF{ 1,1,1 });

			// ボタン
			const Vec2 c = Scene::Center();
			const double w = 220;

			const bool start = SimpleGUI::Button(U"スタート", Vec2{ c.x - w * 0.5, c.y - 20 }, w);
			const bool quit = SimpleGUI::Button(U"終了", Vec2{ c.x - w * 0.5, c.y + 36 }, w);
			const bool enter = KeyEnter.down() || KeySpace.down();

			if (start || enter) {
				if (uiButton) uiButton.playOneShot(0.8);
				if (!gameInitialized) {
					G.layout();
					G.buildMapForStage(1);
					gameInitialized = true;
				}
				state = AppState::Playing;

				// ゲームBGMへ切替
				if (bgmTitle) {
					bgmTitle.stop();
				}
				if (bgmGame && !bgmGame.isPlaying()) {
					bgmGame.setLoop(true);
					bgmGame.setVolume(1.0);    // 任意（既定1.0）
					bgmGame.play();            // 引数なしで再生
					// フェードインしたい場合は以下のように:
					// bgmGame.setVolume(0.0);
					// bgmGame.play();
					// bgmGame.fadeVolume(1.0, 0.6s); // 0.6秒で音量1.0へ
				}
			}
			if (quit || KeyEscape.down()) {
				if (uiButton) uiButton.playOneShot(0.8);
				System::Exit();
			}
			continue; // ゲーム更新はスキップ
		}

		// ここからゲーム本編
		G.layout();
		G.updateEffectsEveryFrame(dtReal);

		if (G.phase == Phase::Planning) {
			G.updatePlanning();
		}
		else if (G.phase == Phase::Simulating) {
			G.updateSimulation(dtReal);
			// 自動フェーズのスキップ（サマリー突入時に演出停止）
			if (SimpleGUI::Button(U"スキップ", Vec2{ Scene::Width() - UIWidth + 24, Scene::Height() - 48 }, 120)) {
				if (uiButton) uiButton.playOneShot(0.8);
				G.phase = Phase::Summary;
				G.stageCleared = G.isBlueWin() && !G.isBlueLose();
				G.clearShakeAndHitStop();
			}
		}
		else {
			G.updateSummary();
		}

		// 盤面描画（シェイク適用）
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
		// UI系（シェイク非適用）
		G.drawUI();
		G.drawHoverHelp();
		G.drawStageBanner();

		auto [bp, rp] = Ownership(G.brd);
		const bool blueLose = G.isBlueLose();
		const bool blueWin = G.isBlueWin();
		if (blueLose || blueWin) {
			const String msg = blueLose ? U"敗北条件達成" : U"勝利条件達成";
			FontAsset(U"UI")(msg).drawAt(28, Vec2{ Scene::Width() * 0.5, 26 }, ColorF{ 1, 1, 0.8 });
		}
	}
}
