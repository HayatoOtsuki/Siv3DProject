# include <Siv3D.hpp> // Siv3D v0.6.16
#include "Game.h"

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
