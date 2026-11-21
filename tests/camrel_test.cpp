// Tests for camrel.h: the moved matrix, the middle and the copy, which buffer a draw bound, the registry,
// the registration rules, and what it is all for: depth that holds up far from the origin.
// g++ -std=c++11 -Wall -Wextra -fsanitize=address,undefined -I src tests/camrel_test.cpp -o camrel_test

#include "camrel.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

static int s_checks = 0;
static int s_failed = 0;

static void Check(bool ok, const char* what) {
	s_checks++;
	if (!ok) {
		s_failed++;
		std::printf("FAILED: %s\n", what);
	}
}

static uint32_t s_seed = 12345;
static double Random() {        // 0..1, the same every run
	s_seed = s_seed * 1664525u + 1013904223u;
	return (double)(s_seed >> 8) / 16777216.0;
}

static CamRel::Matrix Identity() {
	CamRel::Matrix m;
	for (int r = 0; r < 4; r++) {
		for (int c = 0; c < 4; c++) {
			m.m[r][c] = r == c ? 1.0f : 0.0f;
		}
	}
	return m;
}

static void TestIdentity() {
	Check(CamRel::IsIdentity(Identity()), "the identity is the identity");
	int caught = 0;
	for (int r = 0; r < 4; r++) {
		for (int c = 0; c < 4; c++) {
			CamRel::Matrix m = Identity();
			m.m[r][c] += 1e-3f;
			caught += !CamRel::IsIdentity(m);
		}
	}
	Check(caught == 16, "any one number off and it is not the identity");
	CamRel::Matrix z = Identity();
	z.m[3][0] = -0.0f;
	Check(CamRel::IsIdentity(z), "a minus zero is still a zero");
}

static void TestShifted() {
	// T(c) M against the product written out in double, for random matrices and middles
	double worst = 0.0;
	bool rowsKept = true;
	for (int t = 0; t < 200; t++) {
		CamRel::Matrix m;
		for (int r = 0; r < 4; r++) {
			for (int c = 0; c < 4; c++) {
				m.m[r][c] = (float)(r == 3 ? (Random() - 0.5) * 80000.0 : (Random() - 0.5) * 2.0);
			}
		}
		const double c[3] = { std::floor((Random() - 0.5) * 80000.0), std::floor((Random() - 0.5) * 2000.0), std::floor((Random() - 0.5) * 80000.0) };
		const CamRel::Matrix s = CamRel::Shifted(m, c);
		const double t4[4][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { c[0], c[1], c[2], 1 } };
		for (int r = 0; r < 4; r++) {
			for (int j = 0; j < 4; j++) {
				double v = 0.0;
				for (int k = 0; k < 4; k++) {
					v += t4[r][k] * (double)m.m[k][j];
				}
				const double err = std::fabs((double)s.m[r][j] - v) / (1.0 + std::fabs(v));
				if (err > worst) {
					worst = err;
				}
				if (r < 3 && s.m[r][j] != m.m[r][j]) {
					rowsKept = false;
				}
			}
		}
	}
	Check(rowsKept, "the first three rows stay as they are");
	Check(worst < 1e-6, "the fourth row is T(c) M, rounded once");

	// a turned view from far out, moved by a middle near the camera: the fourth row is the small difference,
	// as near to the exact one as float can hold it (float sums of the big numbers would be ~0.004 off)
	double worstNear = 0.0;
	for (int t = 0; t < 500; t++) {
		const double yaw = Random() * 6.2831853, pitch = (Random() - 0.5) * 1.2;
		const double fw[3] = { std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch) };
		const double rt[3] = { std::cos(yaw), 0.0, -std::sin(yaw) };
		const double up[3] = { fw[1] * rt[2] - fw[2] * rt[1], fw[2] * rt[0] - fw[0] * rt[2], fw[0] * rt[1] - fw[1] * rt[0] };
		const double eye[3] = { 30000.0 + Random() * 10000.0, Random() * 500.0, -15000.0 + Random() * 30000.0 };
		CamRel::Matrix v;
		for (int k = 0; k < 3; k++) {
			v.m[k][0] = (float)rt[k];
			v.m[k][1] = (float)up[k];
			v.m[k][2] = (float)fw[k];
			v.m[k][3] = 0.0f;
		}
		v.m[3][0] = (float)-(rt[0] * eye[0] + rt[1] * eye[1] + rt[2] * eye[2]);
		v.m[3][1] = (float)-(up[0] * eye[0] + up[1] * eye[1] + up[2] * eye[2]);
		v.m[3][2] = (float)-(fw[0] * eye[0] + fw[1] * eye[1] + fw[2] * eye[2]);
		v.m[3][3] = 1.0f;
		const double c[3] = { std::floor(eye[0] + (Random() - 0.5) * 400.0), std::floor(eye[1]), std::floor(eye[2] + (Random() - 0.5) * 400.0) };
		const CamRel::Matrix s = CamRel::Shifted(v, c);
		for (int j = 0; j < 3; j++) {
			long double ref = (long double)v.m[3][j];
			for (int k = 0; k < 3; k++) {
				ref += (long double)c[k] * (long double)v.m[k][j];
			}
			const double err = (double)std::fabs((long double)s.m[3][j] - ref) / (1.0 + (double)std::fabs(ref));
			if (err > worstNear) {
				worstNear = err;
			}
		}
	}
	Check(worstNear < 1e-6, "a far view moved by a near middle: the small difference, rounded once");

	// what it is for: a far camera and a far middle cancel in double. The fourth row of a view at 37000
	// moved by a middle near it is the small difference, to float's precision of that small number.
	CamRel::Matrix view = Identity();
	view.m[3][0] = -37012.375f;
	view.m[3][1] = -51.5f;
	view.m[3][2] = -12004.0625f;
	const double c[3] = { 37000.0, 50.0, 12000.0 };
	const CamRel::Matrix moved = CamRel::Shifted(view, c);
	Check(moved.m[3][0] == -12.375f && moved.m[3][1] == -1.5f && moved.m[3][2] == -4.0625f, "a far view and a far middle cancel exactly");
}

static void Put(std::vector<uint8_t>& v, uint32_t i, uint32_t stride, float x, float y, float z) {
	std::memcpy(&v[(size_t)i * stride + 0], &x, 4);
	std::memcpy(&v[(size_t)i * stride + 4], &y, 4);
	std::memcpy(&v[(size_t)i * stride + 8], &z, 4);
}

static void TestCenter() {
	const uint32_t stride = 24;
	std::vector<uint8_t> v(stride * 4, 0xAB);
	Put(v, 0, stride, 30000.25f, -10.0f, 12000.0f);
	Put(v, 1, stride, 30100.75f, 90.0f, 12500.0f);
	Put(v, 2, stride, 30050.0f, 40.0f, 12250.0f);
	Put(v, 3, stride, 30020.0f, 0.0f, 12001.0f);
	double c[3], extent = 0.0;
	Check(CamRel::Center(&v[0], 4, stride, c, &extent), "a middle for four points");
	Check(c[0] == 30051.0 && c[1] == 40.0 && c[2] == 12250.0, "the middle of their box, in whole units");
	Check(std::fabs(extent - 250.0) < 1e-9, "the farthest any point is from it along an axis");
	Check(!CamRel::Center(&v[0], 0, stride, c, &extent), "no points, no middle");
	Check(!CamRel::Center(&v[0], 4, 8, c, &extent), "a vertex too small for a position, no middle");
	Check(!CamRel::Center(NULL, 4, stride, c, &extent), "no buffer, no middle");
	Put(v, 2, stride, std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f);
	Check(!CamRel::Center(&v[0], 4, stride, c, &extent), "a number that is not a number: no middle");
	Put(v, 2, stride, 0.0f, std::numeric_limits<float>::infinity(), 0.0f);
	Check(!CamRel::Center(&v[0], 4, stride, c, &extent), "an infinite number: no middle");
}

static void TestRebaseCopy() {
	const uint32_t stride = 36, count = 50;
	std::vector<uint8_t> src(stride * count), dst(stride * count, 0);
	for (size_t i = 0; i < src.size(); i++) {
		src[i] = (uint8_t)(i * 7 + 3);
	}
	for (uint32_t i = 0; i < count; i++) {
		Put(src, i, stride, (float)(33000.0 + Random() * 800.0), (float)(Random() * 300.0 - 100.0), (float)(-12000.0 - Random() * 800.0));
	}
	const double c[3] = { 33400.0, 50.0, -12400.0 };
	CamRel::RebaseCopy(&dst[0], &src[0], count, stride, c);
	bool exact = true, rounded = true, rest = true;
	for (uint32_t i = 0; i < count; i++) {
		for (int k = 0; k < 3; k++) {
			float s, d;
			std::memcpy(&s, &src[(size_t)i * stride + 4 * k], 4);
			std::memcpy(&d, &dst[(size_t)i * stride + 4 * k], 4);
			// far out (x and z here) a point and its middle are floats of the same size, and their
			// difference is itself a float: nothing is lost. Elsewhere it is rounded once.
			const double diff = (double)s - c[k];
			if (k != 1 && (double)d != diff) {
				exact = false;
			}
			if (std::fabs((double)d - diff) > std::ldexp(std::fabs(diff), -24)) {
				rounded = false;
			}
		}
		if (std::memcmp(&dst[(size_t)i * stride + 12], &src[(size_t)i * stride + 12], stride - 12) != 0) {
			rest = false;
		}
	}
	Check(exact, "far out, the points made relative to the middle lose nothing");
	Check(rounded, "and everywhere they are rounded once at most");
	Check(rest, "the rest of each vertex is copied as it was");
}

static void TestBoundBuffer() {
	Check(CamRel::BoundBuffer(0, 0) == 0 && CamRel::BoundBuffer(5, 1) == 0, "one buffer (or none): that one");
	Check(CamRel::BoundBuffer(1, 3) == 0 && CamRel::BoundBuffer(2, 3) == 1 && CamRel::BoundBuffer(0, 3) == 2,
		"more than one: the one before the index the game moved on to");
}

static void TestRegistry() {
	CamRel::Registry r;
	int a = 0, b = 0;
	CamRel::Entry e;
	std::memset(&e, 0, sizeof(e));
	e.vbuf = &a;
	e.c[0] = 1.0;
	e.stride = 24;
	e.size = 240;
	r.Put(&a, e);
	e.vbuf = &b;
	e.c[0] = 2.0;
	r.Put(&b, e);
	Check(r.Count() == 2, "two put, two there");
	Check(r.Find(&a) && r.Find(&a)->c[0] == 1.0 && r.Find(&a)->vbuf == &a, "found by its key, with what was put");
	Check(r.Find(&e) == NULL, "a key never put is not found");
	e.c[0] = 3.0;
	r.Put(&a, e);
	Check(r.Count() == 2 && r.Find(&a)->c[0] == 3.0, "put again: replaced");
	int seen = 0;
	r.ForEach([&seen](const void*, const CamRel::Entry&) { seen++; });
	Check(seen == 2, "each one gone through once");
	Check(r.Erase(&a) && !r.Find(&a) && r.Count() == 1, "erased: gone");
	Check(!r.Erase(&a), "erased twice: nothing the second time");
	r.Clear();
	Check(r.Count() == 0 && !r.Find(&b), "cleared");
}

static CamRel::MeshInfo Plain() {
	CamRel::MeshInfo m;
	m.buffers = 1;
	m.animated = 0;
	m.wibble = 0;
	m.matrix = -1;
	m.fvf = 0x152;          // position, normal, colour, one set of texture coordinates
	m.stride = 36;
	m.vbufFlags = 0;
	m.usage = 0;
	m.hasCopy = true;
	m.hasBuffer = true;
	m.size = 36 * 100;
	return m;
}

static void TestQualifies() {
	Check(CamRel::Qualifies(Plain()), "a plain static mesh qualifies");
	CamRel::MeshInfo m;
	int caught = 0, tried = 0;
	m = Plain(); m.buffers = 2; caught += !CamRel::Qualifies(m); tried++;
	m = Plain(); m.animated = 1; caught += !CamRel::Qualifies(m); tried++;
	m = Plain(); m.wibble = 0x1234; caught += !CamRel::Qualifies(m); tried++;
	m = Plain(); m.matrix = 3; caught += !CamRel::Qualifies(m); tried++;
	m = Plain(); m.fvf = 0x144; caught += !CamRel::Qualifies(m); tried++;      // already transformed (XYZRHW)
	m = Plain(); m.fvf = 0x156; caught += !CamRel::Qualifies(m); tried++;      // a blend weight after the position
	m = Plain(); m.stride = 8; caught += !CamRel::Qualifies(m); tried++;
	m = Plain(); m.stride = 30; caught += !CamRel::Qualifies(m); tried++;
	m = Plain(); m.vbufFlags = 1; caught += !CamRel::Qualifies(m); tried++;
	m = Plain(); m.vbufFlags = 2; caught += !CamRel::Qualifies(m); tried++;
	m = Plain(); m.usage = 0x208; caught += !CamRel::Qualifies(m); tried++;
	m = Plain(); m.hasCopy = false; caught += !CamRel::Qualifies(m); tried++;
	m = Plain(); m.hasBuffer = false; caught += !CamRel::Qualifies(m); tried++;
	m = Plain(); m.size = 20; caught += !CamRel::Qualifies(m); tried++;
	Check(caught == tried, "each rule on its own keeps a mesh out");
	m = Plain(); m.vbufFlags = 4; m.usage = 8;
	Check(CamRel::Qualifies(m), "a part of a shared buffer, written once, qualifies");

	const double nearC[3] = { 1000.0, -900.0, 1023.0 }, farX[3] = { -1024.0, 0.0, 0.0 }, farZ[3] = { 0.0, 0.0, 30000.0 };
	Check(!CamRel::FarEnough(nearC), "a middle near the origin stays as it is");
	Check(CamRel::FarEnough(farX) && CamRel::FarEnough(farZ), "a middle out along any axis is moved");
}

// The pipeline as the card has it: the view and projection made one float matrix, then a float point
// through it and the divide. The depth that comes out decides what is in front.
struct Mat {
	float m[4][4];
};

static Mat Mul(const Mat& a, const Mat& b) {
	Mat o;
	for (int r = 0; r < 4; r++) {
		for (int c = 0; c < 4; c++) {
			float s = 0.0f;
			for (int k = 0; k < 4; k++) {
				s += a.m[r][k] * b.m[k][c];
			}
			o.m[r][c] = s;
		}
	}
	return o;
}

static float Depth(const Mat& m, float x, float y, float z) {
	const float cz = x * m.m[0][2] + y * m.m[1][2] + z * m.m[2][2] + m.m[3][2];
	const float cw = x * m.m[0][3] + y * m.m[1][3] + z * m.m[2][3] + m.m[3][3];
	return cz / cw;
}

static void TestDepthFarOut() {
	// projection as the game has it (THUG-SDL's far plane): near 2, far 96000, 72 degrees across
	const double n = 2.0, f = 96000.0, ys = 1.0 / std::tan(72.0 * 3.14159265358979 / 360.0);
	Mat proj;
	std::memset(&proj, 0, sizeof(proj));
	proj.m[0][0] = (float)(ys * 0.5625);
	proj.m[1][1] = (float)ys;
	proj.m[2][2] = (float)(f / (f - n));
	proj.m[2][3] = 1.0f;
	proj.m[3][2] = (float)(-n * f / (f - n));

	int lostOld = 0, lostNew = 0, trials = 0;
	for (int t = 0; t < 2000; t++) {
		// a camera out in Tampa, looking somewhere level-ish
		const double eye[3] = { 36000.0 + Random() * 3000.0, 40.0 + Random() * 200.0, 9000.0 + Random() * 6000.0 };
		const double yaw = Random() * 6.2831853, pitch = (Random() - 0.5) * 0.6;
		const double fw[3] = { std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch) };
		const double rt[3] = { std::cos(yaw), 0.0, -std::sin(yaw) };
		const double up[3] = { fw[1] * rt[2] - fw[2] * rt[1], fw[2] * rt[0] - fw[0] * rt[2], fw[0] * rt[1] - fw[1] * rt[0] };
		CamRel::Matrix view;
		for (int k = 0; k < 3; k++) {
			view.m[k][0] = (float)rt[k];
			view.m[k][1] = (float)up[k];
			view.m[k][2] = (float)fw[k];
			view.m[k][3] = 0.0f;
		}
		view.m[3][0] = (float)-(rt[0] * eye[0] + rt[1] * eye[1] + rt[2] * eye[2]);
		view.m[3][1] = (float)-(up[0] * eye[0] + up[1] * eye[1] + up[2] * eye[2]);
		view.m[3][2] = (float)-(fw[0] * eye[0] + fw[1] * eye[1] + fw[2] * eye[2]);
		view.m[3][3] = 1.0f;

		// a wall facing the camera somewhere in view, 200 to 800 out, and a decal on it 0.05 nearer;
		// wall and decal in meshes of their own, with middles a few hundred units off
		const double d = 200.0 + Random() * 600.0, side = (Random() - 0.5) * 0.6 * d, lift = (Random() - 0.5) * 0.3 * d;
		double wall[3], decal[3], cw[3], cd[3];
		for (int k = 0; k < 3; k++) {
			wall[k] = eye[k] + fw[k] * d + rt[k] * side + up[k] * lift;
			decal[k] = wall[k] - fw[k] * 0.05;
			cw[k] = std::floor(wall[k] + (Random() - 0.5) * 600.0);
			cd[k] = std::floor(wall[k] + (Random() - 0.5) * 600.0);
		}
		const float wf[3] = { (float)wall[0], (float)wall[1], (float)wall[2] };
		const float df[3] = { (float)decal[0], (float)decal[1], (float)decal[2] };

		// as the game draws now: world units through one float view-projection
		Mat v;
		std::memcpy(&v, &view, sizeof(v));
		const Mat vp = Mul(v, proj);
		const bool oldOk = Depth(vp, df[0], df[1], df[2]) < Depth(vp, wf[0], wf[1], wf[2]);

		// relative to each mesh's middle, its view moved in double
		const CamRel::Matrix vw = CamRel::Shifted(view, cw), vd = CamRel::Shifted(view, cd);
		Mat mw, md;
		std::memcpy(&mw, &vw, sizeof(mw));
		std::memcpy(&md, &vd, sizeof(md));
		const Mat pw = Mul(mw, proj), pd = Mul(md, proj);
		const float zw = Depth(pw, (float)(wf[0] - cw[0]), (float)(wf[1] - cw[1]), (float)(wf[2] - cw[2]));
		const float zd = Depth(pd, (float)(df[0] - cd[0]), (float)(df[1] - cd[1]), (float)(df[2] - cd[2]));
		const bool newOk = zd < zw;
		trials++;
		lostOld += !oldOk;
		lostNew += !newOk;
	}
	std::printf("a decal 0.05 off its wall, 200..800 out, the camera 36000..39000 from the origin: in front %d of %d drawn the old way, %d of %d relative to the middles\n",
		trials - lostOld, trials, trials - lostNew, trials);
	Check(lostOld * 5 > trials, "drawn the old way, far out, a close decal is lost often (the reason for all this)");
	Check(lostNew * 100 < trials, "relative to the middles it nearly always stays in front");
}

int main() {
	TestIdentity();
	TestShifted();
	TestCenter();
	TestRebaseCopy();
	TestBoundBuffer();
	TestRegistry();
	TestQualifies();
	TestDepthFarOut();
	std::printf("%d checks, %d failed\n", s_checks, s_failed);
	return s_failed ? 1 : 0;
}
