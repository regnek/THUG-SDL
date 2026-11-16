"""Look at the four hooks of src/flickit_game.cpp in a built thugsdl.dll.

usage: check_hooks.py <built thugsdl.dll> [<built thugsdl.dll> ...]

The hooks are called by the game's own code, at places where a call or a compare used to be. For that
they have to keep to what the game's code expects there, whatever the compiler makes of the source:

  rail catch    a bare function: saves ecx and edx, passes esi, answers in al, returns with a plain ret
  grind pick    takes the place of a call to a function that pops five arguments: every return is ret 14h
                (or a jump into the game's function, which does the popping)
  rail balance  takes the place of a call to a function without arguments: every return is a plain ret
                (this one also carries the drag on a rail)
  rail boost    a bare function in the place of a call to GetPhysicsFloat: passes esi to a function
                that answers with the address to go on at, and jumps there with the stack as it found
                it. That address is the game's GetPhysicsFloat or a function of the DLL that returns
                a small number the way the game expects a number back (on the x87 stack, plain ret)

The functions are found without symbols: their addresses are in the DLL's relocation table (they are
handed to patchCall), and each one refers to the address of the game function it stands in for. The
disassembly is printed, so that it can be read in the build log. This script fails the build if a
hook is missing, is there more than once, or returns in another way than listed above.
"""
import struct
import sys

import capstone
import pefile

# Addresses in the game that the hooks refer to
DO_GRIND_TRICK = 0x005608C0
BALANCE_UPDATE = 0x0050B9C0
GET_LOCAL_SKATER = 0x004F94F0
GET_PHYSICS_FLOAT = 0x00516110

# push ecx / push edx / push esi / call ... / add esp, 4 / pop edx / pop ecx / ret
RAIL_CATCH_PATTERN = bytes.fromhex("515256e8") + b"...." + bytes.fromhex("83c4045a59c3")
# push esi / call ... / add esp, 4 / jmp eax
RAIL_BOOST_PATTERN = bytes.fromhex("56e8") + b"...." + bytes.fromhex("83c404ffe0")
NAMES = {
    DO_GRIND_TRICK: "do_grind_trick",
    BALANCE_UPDATE: "the balance update",
    GET_LOCAL_SKATER: "GetLocalSkater",
    GET_PHYSICS_FLOAT: "GetPhysicsFloat",
    0x0076A788: "the skate module pointer",
    0x0041BDC0: "GetComponent",
    0x0040FBC0: "CScript::SetScript",
    0x0040F0A0: "CScript::Update",
    0x00570560: "the flip / rotate afters",
    0x004133A0: "LookUpSymbol",
}


class Dll:
    def __init__(self, path):
        self.path = path
        self.pe = pefile.PE(path)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.image = self.pe.get_memory_mapped_image()
        self.text = next(s for s in self.pe.sections if s.Name.rstrip(b"\0") == b".text")
        self.text_start = self.base + self.text.VirtualAddress
        self.text_end = self.text_start + self.text.Misc_VirtualSize
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.md.detail = True

    def in_text(self, va):
        return self.text_start <= va < self.text_end

    def in_image(self, va):
        return self.base <= va < self.base + len(self.image)

    def bytes(self, va, n):
        return self.image[va - self.base:va - self.base + n]

    def u32(self, va):
        return struct.unpack("<I", self.bytes(va, 4))[0]

    def code_pointers(self):
        """Addresses inside .text that the relocation table points at: functions whose address is taken."""
        out = set()
        for block in getattr(self.pe, "DIRECTORY_ENTRY_BASERELOC", []):
            for entry in block.entries:
                if entry.type != 3:     # IMAGE_REL_BASED_HIGHLOW
                    continue
                va = self.base + entry.rva
                if va + 4 > self.base + len(self.image):
                    continue
                target = self.u32(va)
                if self.in_text(target):
                    out.add(target)
        return sorted(out)

    def entry(self, va):
        """Where a function really starts, if va is only a jump to it (a linker thunk)."""
        for _ in range(4):
            head = self.bytes(va, 5)
            if len(head) == 5 and head[0] == 0xE9:
                target = (va + 5 + struct.unpack("<i", head[1:])[0]) & 0xFFFFFFFF
                if self.in_text(target):
                    va = target
                    continue
            break
        return va

    def function(self, start, limit=0x1800):
        """Instructions from start to the end of the function: the last return or jump away that no
        branch inside the function jumps past."""
        insns = []
        furthest = start
        for ins in self.md.disasm(self.bytes(start, limit), start):
            insns.append(ins)
            is_jump = ins.mnemonic.startswith("j")
            if is_jump and ins.operands and ins.operands[0].type == capstone.x86.X86_OP_IMM:
                target = ins.operands[0].imm & 0xFFFFFFFF
                if start <= target < start + limit:
                    furthest = max(furthest, target)
            if (ins.mnemonic in ("ret", "retn") or ins.mnemonic == "jmp") and ins.address >= furthest:
                break
            if ins.mnemonic == "int3":
                insns.pop()
                break
        return insns

    def game_refs(self, insns):
        """Game addresses an instruction list refers to: as a number, or through a constant in the DLL."""
        refs = set()
        for ins in insns:
            for op in ins.operands:
                values = []
                if op.type == capstone.x86.X86_OP_IMM:
                    values.append(op.imm & 0xFFFFFFFF)
                elif op.type == capstone.x86.X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0:
                    va = op.mem.disp & 0xFFFFFFFF
                    values.append(va)
                    if self.in_image(va) and va + 4 <= self.base + len(self.image):
                        values.append(self.u32(va))
                for v in values:
                    if v in NAMES:
                        refs.add(v)
        return refs


def show(dll, title, insns):
    print("  %s" % title)
    for ins in insns:
        note = ""
        for v in dll.game_refs([ins]):
            note += "   ; " + NAMES[v]
        print("    %08X  %-8s %s%s" % (ins.address, ins.mnemonic, ins.op_str, note))


def returns(insns):
    out = []
    for ins in insns:
        if ins.mnemonic in ("ret", "retn"):
            out.append(int(ins.op_str, 0) if ins.op_str else 0)
    return out


def matches(dll, va, pattern, wild):
    """The bytes at va are the pattern; wild = (start, end) of the part that may be anything."""
    head = dll.bytes(va, len(pattern))
    return len(head) == len(pattern) and head[:wild[0]] == pattern[:wild[0]] and head[wild[1]:] == pattern[wild[1]:]


def code_values(dll, insns):
    """Addresses inside the DLL's code that an instruction list holds as numbers (not as call or jump targets)."""
    out = set()
    for ins in insns:
        if ins.mnemonic == "call" or ins.mnemonic.startswith("j"):
            continue
        for op in ins.operands:
            values = []
            if op.type == capstone.x86.X86_OP_IMM:
                values.append(op.imm & 0xFFFFFFFF)
            elif op.type == capstone.x86.X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0:
                va = op.mem.disp & 0xFFFFFFFF
                if ins.mnemonic == "lea":
                    values.append(va)       # the address itself is what is loaded
                elif dll.in_image(va) and va + 4 <= dll.base + len(dll.image):
                    values.append(dll.u32(va))
            for v in values:
                if dll.in_text(v):
                    out.add(dll.entry(v))
    return sorted(out)


def small_number(dll, insns):
    """The number a short function puts on the x87 stack before it returns with a plain ret, or None
    if the instructions are anything else."""
    if not insns or len(insns) > 12:
        return None
    rets = returns(insns)
    if not rets or any(r != 0 for r in rets):
        return None
    loads = [ins for ins in insns if ins.mnemonic.startswith("f")]
    if len(loads) != 1:
        return None
    load = loads[0]
    if load.mnemonic == "fldz":
        return 0.0
    if load.mnemonic == "fld1":
        return 1.0
    if load.mnemonic == "fld" and len(load.operands) == 1:
        op = load.operands[0]
        if op.type == capstone.x86.X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0:
            va = op.mem.disp & 0xFFFFFFFF
            if dll.in_image(va) and va + 8 <= dll.base + len(dll.image):
                if op.size == 4:
                    return struct.unpack("<f", dll.bytes(va, 4))[0]
                if op.size == 8:
                    return struct.unpack("<d", dll.bytes(va, 8))[0]
    return None


def check(path):
    dll = Dll(path)
    problems = []
    print("%s" % path)
    pointers = dll.code_pointers()
    print("  %d functions have their address taken" % len(pointers))

    catch, pick, balance, boost = [], [], [], []
    for p in sorted(set(dll.entry(p) for p in pointers)):
        if matches(dll, p, RAIL_CATCH_PATTERN, (4, 8)):
            catch.append(p)
            continue
        if matches(dll, p, RAIL_BOOST_PATTERN, (2, 6)):
            boost.append(p)
            continue
        insns = dll.function(p)
        refs = dll.game_refs(insns)
        if DO_GRIND_TRICK in refs:
            pick.append((p, insns))
        elif BALANCE_UPDATE in refs:
            balance.append((p, insns))

    # --- rail catch
    if len(catch) != 1:
        problems.append("the rail catch hook was found %d times" % len(catch))
    for p in catch:
        show(dll, "rail catch hook at %08X" % p, dll.function(p))
        target = dll.entry((p + 8 + struct.unpack("<i", dll.bytes(p + 4, 4))[0]) & 0xFFFFFFFF)
        if not dll.in_text(target):
            problems.append("the rail catch hook calls %08X, outside the code" % target)
            continue
        callee = dll.function(target)
        show(dll, "which calls %08X" % target, callee)
        rets = returns(callee)
        if not rets or any(r != 0 for r in rets):
            problems.append("the function the rail catch hook calls returns with %s, not with a plain ret" % rets)

    # --- grind pick
    if len(pick) != 1:
        problems.append("the grind pick hook was found %d times" % len(pick))
    for p, insns in pick:
        show(dll, "grind pick hook at %08X" % p, insns)
        rets = returns(insns)
        tail = insns and insns[-1].mnemonic == "jmp"
        if any(r != 0x14 for r in rets) or (not rets and not tail):
            problems.append("the grind pick hook returns with %s: every return has to pop 0x14 bytes" % [hex(r) for r in rets])

    # --- rail balance
    if len(balance) != 1:
        problems.append("the rail balance hook was found %d times" % len(balance))
    for p, insns in balance:
        show(dll, "rail balance hook at %08X" % p, insns)
        rets = returns(insns)
        if not rets or any(r != 0 for r in rets):
            problems.append("the rail balance hook returns with %s, not with a plain ret" % rets)

    # --- rail boost
    if len(boost) != 1:
        problems.append("the rail boost hook was found %d times" % len(boost))
    for p in boost:
        show(dll, "rail boost hook at %08X" % p, dll.function(p))
        target = dll.entry((p + 6 + struct.unpack("<i", dll.bytes(p + 2, 4))[0]) & 0xFFFFFFFF)
        if not dll.in_text(target):
            problems.append("the rail boost hook calls %08X, outside the code" % target)
            continue
        callee = dll.function(target)
        show(dll, "which calls %08X" % target, callee)
        rets = returns(callee)
        if not rets or any(r != 0 for r in rets):
            problems.append("the function the rail boost hook calls returns with %s, not with a plain ret" % rets)
        if GET_PHYSICS_FLOAT not in dll.game_refs(callee):
            problems.append("the function the rail boost hook calls does not hand out the game's GetPhysicsFloat")
        small = []
        for v in code_values(dll, callee):
            body = dll.function(v)
            number = small_number(dll, body)
            if number is not None:
                small.append((v, body, number))
        if len(small) != 1:
            problems.append("the function that answers for the rail boost in the game's place was found %d times" % len(small))
        for v, body, number in small:
            show(dll, "or hands out %08X, which answers %g" % (v, number), body)
            if not 0.0 < number <= 20.0:
                problems.append("the rail boost that replaces the game's is %g, not a small speed" % number)

    for problem in problems:
        print("::error::%s: %s" % (path, problem))
    print("  %s" % ("FAILED" if problems else "ok"))
    return not problems


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    ok = True
    for path in sys.argv[1:]:
        ok = check(path) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
