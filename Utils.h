#pragma once
#include <Siv3D.hpp>

// ===================== ユーティリティ（Clamp 使わない） =====================
template <class T>
inline T limit(const T& x, const T& lo, const T& hi) {
	return (x < lo ? lo : (x > hi ? hi : x));
}
inline double saturate(double x) { return limit(x, 0.0, 1.0); }

// 簡易イージング（バナー表示など）
inline double easeOutBack(double t) {
	t = saturate(t);
	const double c1 = 1.70158, c3 = c1 + 1.0;
	return 1 + c3 * Pow(t - 1, 3) + c1 * Pow(t - 1, 2);
}
inline double easeInOutSine(double t) {
	t = saturate(t);
	return -0.5 * (Cos(s3d::Math::Pi * t) - 1.0);
}
