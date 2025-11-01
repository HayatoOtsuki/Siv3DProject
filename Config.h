#pragma once

// ===================== マップ設定 =====================
inline constexpr int GW = 36;
inline constexpr int GH = 20;

// 画面レイアウト
inline constexpr double UIWidth = 360.0;
inline constexpr double Margin = 12.0;

// ターン/シミュレーション
inline constexpr double SimDuration = 10.0;

// 経済
inline constexpr int CostBasic = 80;
inline constexpr int CostSprinkler = 60;
inline constexpr int CostPump = 100;
inline constexpr int CostSniper = 160;
inline constexpr int CostMortar = 200;
inline constexpr int CostSpawner = 200;
inline constexpr int IncomePerTile = 1;
inline constexpr int IncomePerPump = 15;

// 構造物 HP
inline constexpr double HPBasic = 120.0;
inline constexpr double HPSprinkler = 50.0;
inline constexpr double HPPump = 250.0;
inline constexpr double HPSniper = 130.0;
inline constexpr double HPMortar = 150.0;
inline constexpr double HPHQ = 600.0;
inline constexpr double HPSpawner = 140.0;

// プレイヤー（出撃ユニット）
inline constexpr int    HPPlayer = 80;
inline constexpr double PlayerSpeed = 70.0;
inline constexpr double PlayerRadius = 10.0;
inline constexpr double PlayerPaintPerSecond = 0.35;
inline constexpr double PlayerLifetime = 10.0;
// プレイヤー爆発
inline constexpr int    PlayerExplodeRadius = 2;
inline constexpr double PlayerExplodeDamage = 50.0;
inline constexpr double PlayerExplodePaint = 0.60;
inline constexpr double PlayerExplodeShakePow = 16.0;
inline constexpr double PlayerExplodeShakeDur = 0.22;
inline constexpr double PlayerExplodeHitstop = 0.12;

// 敵ユニット
inline constexpr int    HPEnemy = 80;
inline constexpr double EnemySpeed = 70.0;
inline constexpr double EnemyRadius = 10.0;
inline constexpr double EnemyLifetime = 10.0;
// 敵爆発
inline constexpr int    EnemyExplodeRadius = 2;
inline constexpr double EnemyExplodeDamage = 50.0;
inline constexpr double EnemyExplodePaint = 0.55;
inline constexpr double EnemyExplodeShakePow = 12.0;
inline constexpr double EnemyExplodeShakeDur = 0.18;
inline constexpr double EnemyExplodeHitstop = 0.04;

// 敵スポナーの出撃間隔
inline constexpr double EnemySpawnerInterval = 16.0; //初期値4.0f
