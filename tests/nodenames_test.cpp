// Tests of the node name table (src/nodenames.h) that takes the place of the game's.
//
// The game's own table is re-expressed here from its code (0x00530F50 fills it, 0x00530AE0 looks a
// name up): 16384 two-byte slots, a node in at the low 14 bits of its name or the next free slot,
// a lookup that reads each slot back as a signed 16-bit number and the node's name before it answers.
// Up to the game's limits the new table has to answer exactly as that one does; past them it has to
// keep answering right, where the game's would hang or go wrong.
#include "nodenames.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

static int g_checks = 0;
static int g_failures = 0;
static const char* g_case = "";

#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failures++; std::printf("  FAIL [%s] line %d: %s\n", g_case, __LINE__, #cond); } } while (0)

// A level's nodes as far as the table cares: a name or none
struct Nodes {
	std::vector<uint32_t> name;
	std::vector<bool> named;
	uint32_t Count() const { return (uint32_t)name.size(); }
	void Add(uint32_t n, bool has = true) { name.push_back(n); named.push_back(has); }
};

static bool NameOf(void* context, uint32_t index, uint32_t* out) {
	const Nodes* nodes = (const Nodes*)context;
	if (index >= nodes->Count()) {
		std::printf("  FAIL [%s]: node %u asked for, only %u\n", g_case, index, nodes->Count());
		g_failures++;
		return false;
	}
	*out = nodes->named[index] ? nodes->name[index] : 0;
	return nodes->named[index];
}

// The game's table (only for fewer than 16384 named nodes: it has no check for a full one)
struct GameTable {
	uint16_t slot[0x4000];
	void Build(const Nodes& nodes) {
		std::memset(slot, 0xFF, sizeof(slot));
		for (uint32_t i = 0; i < nodes.Count(); i++) {
			if (!nodes.named[i]) {
				continue;
			}
			uint32_t at = nodes.name[i] & 0x3FFF;
			while (slot[at] != 0xFFFF) {
				at = (at + 1) & 0x3FFF;
			}
			slot[at] = (uint16_t)i;
		}
	}
	int32_t Find(const Nodes& nodes, uint32_t name) const {
		for (uint32_t at = name & 0x3FFF; slot[at] != 0xFFFF; at = (at + 1) & 0x3FFF) {
			const int32_t index = (int16_t)slot[at];
			if (nodes.named[index] && nodes.name[index] == name) {
				return index;
			}
		}
		return -1;
	}
};

// A different odd name for every number: odd times odd is odd, and multiplying by an odd number
// is undone by its inverse (mod 2^32), so no two numbers share a name
static uint32_t OddName(uint32_t i) {
	return (2u * i + 1u) * 0x9E3779B1u;
}

static uint32_t g_seed = 0x2545F491u;
static uint32_t Random() {
	g_seed ^= g_seed << 13;
	g_seed ^= g_seed >> 17;
	g_seed ^= g_seed << 5;
	return g_seed;
}

// Up to the game's limits both tables give the same answers: names used once, names used several
// times, nodes without names, names nobody has, and many names sharing low bits
static void SameAsTheGame() {
	g_case = "same answers as the game's table";
	static GameTable game;
	for (int round = 0; round < 40; round++) {
		Nodes nodes;
		// every fourth round, fewer nodes whose names share their low 14 bits in bunches of about
		// four (long runs of full slots in the game's table)
		const bool bunched = round % 4 == 3;
		const uint32_t count = 1 + Random() % (bunched ? 4000 : 16000);
		const uint32_t pool = 1 + Random() % (count * 2);
		std::vector<uint32_t> names(pool);
		for (uint32_t k = 0; k < pool; k++) {
			names[k] = bunched ? ((Random() << 14) | (Random() % 1024)) : Random();
		}
		for (uint32_t i = 0; i < count; i++) {
			nodes.Add(names[Random() % pool], Random() % 10 != 0);
		}
		game.Build(nodes);
		NodeNames::Table table;
		CHECK(table.Build(nodes.Count(), NameOf, &nodes));
		CHECK(table.Size() >= 2 * count);
		int differ = 0;
		for (uint32_t k = 0; k < pool; k++) {
			if (table.Find(names[k], count, NameOf, &nodes) != game.Find(nodes, names[k])) {
				differ++;
			}
		}
		for (int k = 0; k < 200; k++) {
			const uint32_t name = Random();
			if (table.Find(name, count, NameOf, &nodes) != game.Find(nodes, name)) {
				differ++;
			}
		}
		CHECK(differ == 0);
	}
}

// Past the game's limits: 60000 nodes, every one found where it is, also past 32767
static void PastTheGamesLimits() {
	g_case = "60000 nodes";
	Nodes nodes;
	for (uint32_t i = 0; i < 60000; i++) {
		nodes.Add(OddName(i));   // odd names, so that the even ones below are nobody's
	}
	// the names must differ for "found where it is" to mean anything
	std::vector<uint32_t> sorted(nodes.name);
	std::sort(sorted.begin(), sorted.end());
	const bool unique = std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end();
	CHECK(unique);
	NodeNames::Table table;
	CHECK(table.Build(nodes.Count(), NameOf, &nodes));
	CHECK(table.Named() == 60000);
	CHECK(table.Size() >= 120000);
	int wrong = 0;
	for (uint32_t i = 0; i < nodes.Count(); i++) {
		if (table.Find(nodes.name[i], nodes.Count(), NameOf, &nodes) != (int32_t)i) {
			wrong++;
		}
	}
	CHECK(wrong == 0);
	CHECK(table.Find(nodes.name[50000], nodes.Count(), NameOf, &nodes) == 50000);
	int found = 0;
	for (int k = 0; k < 10000; k++) {
		if (table.Find(Random() & ~1u, nodes.Count(), NameOf, &nodes) != -1) {
			found++;
		}
	}
	CHECK(found == 0);
}

// Of several nodes with one name the first in the array wins; nodes without a name are never found
static void FirstOfOneName() {
	g_case = "one name, several nodes";
	Nodes nodes;
	for (uint32_t i = 0; i < 50000; i++) {
		nodes.Add(OddName(i));
	}
	const uint32_t shared = 0x12345678u;   // even: no other node has it
	nodes.name[40000] = shared;
	nodes.name[100] = shared;
	nodes.name[49999] = shared;
	nodes.named[7] = false;
	NodeNames::Table table;
	CHECK(table.Build(nodes.Count(), NameOf, &nodes));
	CHECK(table.Named() == 49999);
	CHECK(table.Find(shared, nodes.Count(), NameOf, &nodes) == 100);
	CHECK(table.Find(0, nodes.Count(), NameOf, &nodes) == -1);
	// the array shrinks under the table: numbers past its end are passed over
	CHECK(table.Find(nodes.name[45000], 45000, NameOf, &nodes) == -1);
	CHECK(table.Find(shared, 101, NameOf, &nodes) == 100);
	// a node renamed after the table was made is not found by its old name, and the next one is
	nodes.name[100] = 0x0BADF00Du;
	CHECK(table.Find(shared, nodes.Count(), NameOf, &nodes) == 40000);
}

static void EmptyAndTooBig() {
	g_case = "no nodes, too many nodes";
	Nodes nodes;
	NodeNames::Table table;
	CHECK(table.Find(1, 0, NameOf, &nodes) == -1);             // never built
	CHECK(table.Build(0, NameOf, &nodes));
	CHECK(table.Size() == NodeNames::Table::kLeast);
	CHECK(table.Find(1, 0, NameOf, &nodes) == -1);
	// more than the biggest table holds half full: refused before any node is asked for
	CHECK(!table.Build(NodeNames::Table::kMost / 2 + 1, NameOf, &nodes));
	CHECK(table.Size() == 0);
	CHECK(table.Find(1, 0, NameOf, &nodes) == -1);
	// built big, then small again (the next level)
	Nodes big;
	for (uint32_t i = 0; i < 40000; i++) {
		big.Add(Random());
	}
	CHECK(table.Build(big.Count(), NameOf, &big));
	CHECK(table.Size() == 131072);
	Nodes small;
	small.Add(0xAAAAAAAAu);
	small.Add(0xBBBBBBBBu);
	CHECK(table.Build(small.Count(), NameOf, &small));
	CHECK(table.Size() == NodeNames::Table::kLeast);
	CHECK(table.Find(0xBBBBBBBBu, small.Count(), NameOf, &small) == 1);
	CHECK(table.Find(big.name[0], small.Count(), NameOf, &small) == -1);
}

int main() {
	SameAsTheGame();
	PastTheGamesLimits();
	FirstOfOneName();
	EmptyAndTooBig();
	std::printf("nodenames_test: %d checks, %d failures\n", g_checks, g_failures);
	return g_failures ? 1 : 0;
}
