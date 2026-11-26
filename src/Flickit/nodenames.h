#pragma once
#include <stdint.h>
#include <stdlib.h>

// The game's table from node names to node numbers, with room for a world made of many levels.
//
// THUG keeps it in 16384 two-byte slots at 0x007C08A0. 0x00530F50 fills it each time a level's nodes
// load: every node with a Name goes in at the low 14 bits of the name, or the next free slot after
// that, with no check for a full table. 0x00530AE0 looks a name up the same way and reads each slot
// back as a signed 16-bit number. So more than 16383 named nodes hang the load, and a node numbered
// past 32767 cannot be found. New Jersey and Manhattan joined have 9725 nodes; the nine story levels
// together have 37561.
//
// This table works the same way with four-byte slots, as many as it needs (at least twice the nodes,
// and never fewer than the game's 16384). A node goes in at the low bits of its name or the next free
// slot after that; a lookup starts at the same slot and reads each node's name again before it
// answers. So of two nodes with one name, the first in the array is the one found, as in the game.
// Plain C memory and no exceptions, so that it can be tested anywhere (tests/nodenames_test.cpp).
namespace NodeNames {
	// Writes the name of node number index to *name; false for a node without a name
	typedef bool (*NameOf)(void* context, uint32_t index, uint32_t* name);

	class Table {
	public:
		static const uint32_t kLeast = 16384;      // the game's size
		static const uint32_t kMost = 1u << 22;    // 16 MB of slots, half full at two million nodes

		Table() : m_slots(0), m_size(0), m_named(0) {}
		~Table() { Clear(); }

		// Fills the table from count nodes. False if they need more than kMost slots or there is no
		// memory for them; the table is empty then and every lookup answers -1.
		bool Build(uint32_t count, NameOf nameOf, void* context) {
			uint32_t size = kLeast;
			while (size < kMost && size / 2u < count) {
				size <<= 1;
			}
			if (size / 2u < count) {
				Clear();
				return false;
			}
			if (size != m_size) {
				int32_t* slots = (int32_t*)realloc(m_slots, (size_t)size * sizeof(int32_t));
				if (!slots) {
					Clear();
					return false;
				}
				m_slots = slots;
				m_size = size;
			}
			for (uint32_t i = 0; i < m_size; i++) {
				m_slots[i] = kEmpty;
			}
			m_named = 0;
			const uint32_t mask = m_size - 1;
			for (uint32_t i = 0; i < count; i++) {
				uint32_t name = 0;
				if (!nameOf(context, i, &name)) {
					continue;
				}
				uint32_t at = name & mask;
				while (m_slots[at] != kEmpty) {
					at = (at + 1) & mask;
				}
				m_slots[at] = (int32_t)i;
				m_named++;
			}
			return true;
		}

		// The number of the first node with this name, or -1. count: how many nodes the array has now;
		// a slot holding a number at or past it is passed over, as is one whose node now has another name.
		int32_t Find(uint32_t name, uint32_t count, NameOf nameOf, void* context) const {
			if (!m_size) {
				return -1;
			}
			const uint32_t mask = m_size - 1;
			for (uint32_t at = name & mask; m_slots[at] != kEmpty; at = (at + 1) & mask) {
				const uint32_t index = (uint32_t)m_slots[at];
				uint32_t found = 0;
				if (index < count && nameOf(context, index, &found) && found == name) {
					return (int32_t)index;
				}
			}
			return -1;
		}

		void Clear() {
			free(m_slots);
			m_slots = 0;
			m_size = 0;
			m_named = 0;
		}

		uint32_t Size() const { return m_size; }     // slots
		uint32_t Named() const { return m_named; }   // nodes in it

	private:
		static const int32_t kEmpty = -1;
		int32_t* m_slots;
		uint32_t m_size;
		uint32_t m_named;

		Table(const Table&);
		Table& operator=(const Table&);
	};
}
