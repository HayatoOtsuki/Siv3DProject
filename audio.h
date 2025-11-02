#pragma once
#include <Siv3D.hpp>

namespace MyNamespace
{

	enum class AudioID
	{
		SE_Place,//設置
		SE_ShootBasic,//基本タレット射撃
		SE_ShootSniper,//スナイパー射撃
		SE_MortarLaunch,//迫撃砲発射
		SE_Sprinkler,//スプリンクラー
		SE_Hit,       // 被弾（単発ヒット）
		SE_UIConfirm,//UI決定
		SE_StageClear,//ステージクリア
		SE_GameOver,//ゲームオーバー
	};

	class AudioManager {
	public:
		inline void add(AudioID id, const s3d::FilePath& path, double volume = 1.0, bool allowOverlap = true) {
			using namespace s3d;
			Entry& e = m_table[id];
			e.baseVol = volume;
			e.allowOverlap = allowOverlap;

			e.paths << path;

			if (FileSystem::Exists(path)) {
				Audio a{ path };
				if (a) {
					e.variants << std::move(a);
					e.hasReal = true;
				}
				else {
					Console.writeln(U"[SFX] Decode failed: {}"_fmt(path));
					e.variants << Audio{ Wave(2205) }; // 無音ダミー
				}
			}
			else {
				Console.writeln(U"[SFX] Missing file: {}"_fmt(path));
				e.variants << Audio{ Wave(2205) }; // 無音ダミー
			}
		}

		inline void addVariants(AudioID id, const s3d::Array<s3d::FilePath>& paths, double volume = 1.0, bool allowOverlap = true) {
			for (const auto& p : paths) add(id, p, volume, allowOverlap);
		}

		inline bool has(AudioID id) const {
			const auto it = m_table.find(id);
			return (it != m_table.end() && !it->second.variants.isEmpty());
		}

		inline void play(AudioID id, double volume = 1.0, double pitch = 1.0, double pan = 0.0) {
			using namespace s3d;
			const Entry* e = find(id);
			if (!e || e->variants.isEmpty()) return;

			const Audio& a = e->variants.front();
			const double v = clamp01(volume * e->baseVol * m_master);
			if (e->allowOverlap) a.playOneShot(v, pitch, pan);
			else                 playExclusive_(a, v, pitch, pan);
		}

		inline void playRandom(AudioID id, double volume = 1.0, double pitch = 1.0, double pan = 0.0) {
			using namespace s3d;
			const Entry* e = find(id);
			if (!e || e->variants.isEmpty()) return;

			const Audio& a = e->variants.choice();
			const double v = clamp01(volume * e->baseVol * m_master);
			if (e->allowOverlap) a.playOneShot(v, pitch, pan);
			else                 playExclusive_(a, v, pitch, pan);
		}

		inline void playRandomPitch(AudioID id, double volume = 1.0, double pitchMin = 0.97, double pitchMax = 1.03, double pan = 0.0) {
			const double pitch = s3d::Random(pitchMin, pitchMax);
			playRandom(id, volume, pitch, pan);
		}

		inline void setMasterVolume(double v) { m_master = (v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v)); }
		inline double masterVolume() const { return m_master; }

		inline void loadDefaults(const s3d::FilePath& base = U"rom/audio/") {
			add(AudioID::SE_Place, base + U"SNES-Shooter01-11(Item).mp3", 0.65);
			add(AudioID::SE_ShootBasic, base + U"SNES-Shooter01-01(Shoot).mp3", 0.70);
			add(AudioID::SE_ShootSniper, base + U"SNES-Shooter02-01(Shoot).mp3", 0.70);
			add(AudioID::SE_MortarLaunch, base + U"SNES-Shooter01-04(Shoot).mp3", 0.75);
			add(AudioID::SE_Sprinkler, base + U"SNES-Shooter02-04(Shoot).mp3", 0.60, true);
			add(AudioID::SE_Hit, base + U"SNES-Shooter02-08(Damage).mp3", 0.80, true);
			add(AudioID::SE_UIConfirm, base + U"ボタンを押す44.mp3", 0.70);
			add(AudioID::SE_StageClear, base + U"8bit詠唱4.wav", 0.90);
			add(AudioID::SE_GameOver, base + U"AnyConv.com__se_mero_gameover06.wav", 0.90);

			debugDump();
		}

		// 何がロードできたかを表示（Console）
		inline void debugDump() const {
			using namespace s3d;
			Console.writeln(U"[SFX] --------- Loaded Table ---------");
			for (const auto& [id, e] : m_table) {
				Console.writeln(U"[SFX] id={} variants={} real={} first={}"_fmt(
					(int)id, e.variants.size(), e.hasReal, (e.paths.isEmpty() ? U"" : e.paths.front())));
			}
			Console.writeln(U"[SFX] --------------------------------");
		}

	private:
		struct Entry {
			s3d::Array<s3d::Audio> variants;
			double baseVol = 1.0;
			bool allowOverlap = true;
			bool hasReal = false;                    // 実ファイルがひとつでも読めたか
			s3d::Array<s3d::FilePath> paths;        // デバッグ表示用
		};

		s3d::HashTable<AudioID, Entry> m_table;
		double m_master = 1.0;

		inline const Entry* find(AudioID id) const {
			const auto it = m_table.find(id);
			return (it == m_table.end() ? nullptr : &it->second);
		}

		static inline double clamp01(double v) { return (v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v)); }

		inline void playExclusive_(const s3d::Audio& a, double volume, double pitch, double pan) {
			// 必要なら stop を入れる: if (a.isPlaying()) a.stop(0.02);
			a.playOneShot(volume, pitch, pan);
		}
	};

	// グローバルインスタンス
	inline AudioManager& SFX() {
		static AudioManager g;
		return g;
	}

} // namespace MyNamespace
