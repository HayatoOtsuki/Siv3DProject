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
inline constexpr int CostBasic = 40;
inline constexpr int CostSprinkler = 35;
inline constexpr int CostPump = 60;
inline constexpr int CostSniper = 90;
inline constexpr int CostMortar = 80;
inline constexpr int CostSpawner = 100;
inline constexpr int IncomePerTile = 1;
inline constexpr int IncomePerPump = 20;

// 構造物 HP
inline constexpr double HPBasic = 120.0;
inline constexpr double HPSprinkler = 90.0;
inline constexpr double HPPump = 120.0;
inline constexpr double HPSniper = 80.0;
inline constexpr double HPMortar = 100.0;
inline constexpr double HPHQ = 600.0;
inline constexpr double HPSpawner = 140.0;

// プレイヤー（出撃ユニット）
inline constexpr int    HPPlayer = 200;
inline constexpr double PlayerSpeed = 220.0;
inline constexpr double PlayerRadius = 10.0;
inline constexpr double PlayerPaintPerSecond = 0.35;
inline constexpr double PlayerLifetime = 10.0;
// プレイヤー爆発
inline constexpr int    PlayerExplodeRadius = 2;
inline constexpr double PlayerExplodeDamage = 50.0;
inline constexpr double PlayerExplodePaint = 0.60;
inline constexpr double PlayerExplodeShakePow = 16.0;
inline constexpr double PlayerExplodeShakeDur = 0.22;
inline constexpr double PlayerExplodeHitstop = 0.06;

// 敵ユニット
inline constexpr int    HPEnemy = 200;
inline constexpr double EnemySpeed = 180.0;
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
inline constexpr double EnemySpawnerInterval = 4.0;
