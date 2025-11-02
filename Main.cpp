
# include <Siv3D.hpp> // Siv3D v0.6.16
#include "Game.h"
#include "audio.h"


enum class AppState { Title, Playing };

void Main() {
	Window::Resize(1600, 900);
	Scene::SetBackground(ColorF{ 0.06, 0.06, 0.07 });
	FontAsset::Register(U"UI", FontMethod::MSDF, 20, Typeface::Bold);
	const ScopedRenderStates2D _sampler{ SamplerState::ClampLinear };


	// 効果音をロード
	MyNamespace::SFX().loadDefaults();      
	MyNamespace::SFX().setMasterVolume(0.9);

	Game G;
	AppState state = AppState::Title;
	bool gameInitialized = false;

	s3d::Texture titleImage;
	if (s3d::FileSystem::Exists(U"Title.png")) {
		titleImage = s3d::Texture{ U"Title.png", s3d::TextureDesc::Mipped };
	}

	while (System::Update()) {
		const double dtReal = Scene::DeltaTime();

		// タイトル画面
		if (state == AppState::Title) {
			// 背景とタイトル
			const RectF panel{ 0, 0, (double)Scene::Width(), (double)Scene::Height() };
			panel.draw(ColorF{ 0,0,0,0.15 });

			// 画像がある場合は画像を描画、無い場合は文字をフォールバック描画
			const s3d::Vec2 titlePos = Scene::Center().movedBy(0, 130);
			if (titleImage) {
				// 画像の中心を titlePos に合わせて描画
				titleImage.drawAt(titlePos);
			}
			else {
				// フォールバック（従来のテキスト描画）
				FontAsset(U"UI")(U"浸食！インクウォーズ").drawAt(48, titlePos, ColorF{ 1,1,1 });
			}

			// ボタン
			const Vec2 c = Scene::Center();
			const double w = 220;

			const bool start = SimpleGUI::Button(U"スタート", Vec2{ c.x - w * 0.5, c.y - 20 }, w);
			const bool quit = SimpleGUI::Button(U"終了", Vec2{ c.x - w * 0.5, c.y + 36 }, w);

			// Enter でもスタート
			const bool enter = KeyEnter.down() || KeySpace.down();

			if (start || enter) {
				if (!gameInitialized) {
					G.layout();
					G.buildMapForStage(1);
					gameInitialized = true;
				}
				state = AppState::Playing;
			}
			if (quit || KeyEscape.down()) {
				System::Exit();
			}
			continue; // ゲーム更新はスキップ
		}

		// ここからゲーム本編
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
	}
}
