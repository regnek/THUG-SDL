"""Look at the hooks of src/flickit_game.cpp in a built thugsdl.dll.

usage: check_hooks.py <built thugsdl.dll> [<built thugsdl.dll> ...]

The hooks are called by the game's own code, at places where a call or a compare used to be. For that
they have to keep to what the game's code expects there, whatever the compiler makes of the source:

  rail catch    a bare function: saves ecx and edx, passes esi, answers in al, returns with a plain ret
  rail search   takes the place of four calls to the game's search for a rail, which pops seven
                arguments: every return is ret 1Ch (or a jump into the game's function, which does
                the popping)
  rail test     the function the search here hands every piece of rail within reach to. It calls
                the game's own test of a piece of rail, which takes two vectors on the stack (32
                bytes) and leaves them there: the room is made before the call, the stack is level
                again at the plain ret
  grind pick    takes the place of a call to a function that pops five arguments: every return is ret 14h
                (or a jump into the game's function, which does the popping)
  rail balance  takes the place of a call to a function without arguments: every return is a plain ret
                (this one also carries the drag on a rail)
  air gravity   takes the place of four calls to a function without arguments that answers on the
                floating point stack: every return is a plain ret
  jump          takes the place of the call to do_jump, which pops one argument: every return is
                ret 4 (or a jump into the game's function)
  jump speed    takes the place of a call to CStruct::GetFloat, which pops three arguments: every
                return is ret 0Ch (or a jump into the game's function)
  brake test    takes the place of the call to the game's "should he brake": no arguments, every
                return is a plain ret
  kick test     two functions, in the place of the two calls to the game's "may he kick" (one in the
                physics of the ground, one for the script command CanKick): no arguments, every
                return is a plain ret. The first also reads the skater's animation and asks the
                game where two of his bones are
  friction      takes the place of the call to the game's friction on the ground: no arguments,
                every return is a plain ret
  turning       takes the place of the call to the game's turning on the ground: no arguments, every
                return is a plain ret. It turns the skater with the game's own two functions for that
  held          takes the place of a call to the game's "is this button down", which pops one
                argument: every return is ret 4 (or a jump into the game's function)
  pad update    takes the place of a call to the game's update of its picture of a pad, which pops
                one argument: every return is ret 4 (or a jump into the game's function)
  air turning   takes the place of the call to the game's turning in the air: no arguments, every
                return is a plain ret
  spin rate     takes the place of two calls to the game's "one of the skater's numbers that go by
                his stats", which pops one argument and answers on the floating point stack: every
                return is ret 4 (or a jump into the game's function) with the answer there
  the feet      not a hook: the kick test hook in the physics of the ground, or a function it calls,
                asks the game where two of the skater's bones are. The game's function takes its two
                arguments off the stack itself. The code is printed, and it has to be there once
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
RAIL_SEARCH = 0x00514630
RAIL_VALID = 0x00512760
AIR_GRAVITY = 0x0055E670
DO_JUMP = 0x00560410
STRUCT_GET_FLOAT = 0x00410E20
SHOULD_BRAKE = 0x0055D250
SHOULD_KICK = 0x0055D410
GROUND_FRICTION = 0x00563B80
GROUND_TURN = 0x0055E790
VECTOR_TURN = 0x0042F6D0
MATRIX_TURN = 0x00405230
BUTTON_HELD = 0x00451A70
PAD_UPDATE = 0x0051AEB0
AIR_TURN = 0x0055F050
SCRIPTED_STAT = 0x00516AD0
BONE_POSITION = 0x00487100

# push ecx / push edx / push esi / call ... / add esp, 4 / pop edx / pop ecx / ret
RAIL_CATCH_PATTERN = bytes.fromhex("515256e8") + b"...." + bytes.fromhex("83c4045a59c3")
# push esi / call ... / add esp, 4 / jmp eax
RAIL_BOOST_PATTERN = bytes.fromhex("56e8") + b"...." + bytes.fromhex("83c404ffe0")
NAMES = {
    DO_GRIND_TRICK: "do_grind_trick",
    BALANCE_UPDATE: "the balance update",
    GET_LOCAL_SKATER: "GetLocalSkater",
    GET_PHYSICS_FLOAT: "GetPhysicsFloat",
    RAIL_SEARCH: "the game's search for a rail",
    RAIL_VALID: "the game's test of a piece of rail",
    AIR_GRAVITY: "get_air_gravity",
    DO_JUMP: "do_jump",
    STRUCT_GET_FLOAT: "CStruct::GetFloat",
    SHOULD_BRAKE: "should_brake",
    SHOULD_KICK: "should_kick",
    GROUND_FRICTION: "the friction on the ground",
    GROUND_TURN: "the turning on the ground",
    VECTOR_TURN: "turn a vector",
    MATRIX_TURN: "turn a matrix",
    BUTTON_HELD: "is this button down",
    PAD_UPDATE: "the pad update",
    AIR_TURN: "the turning in the air",
    SCRIPTED_STAT: "the skater's number by his stats",
    0x0076A788: "the skate module pointer",
    0x0041BDC0: "GetComponent",
    0x0040FBC0: "CScript::SetScript",
    0x0040F0A0: "CScript::Update",
    0x00570560: "the flip / rotate afters",
    0x004133A0: "LookUpSymbol",
    BONE_POSITION: "where a bone is",
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


def stack_at_calls(insns):
    """Follows the stack pointer through a function, one instruction after the other. That is right
    for code that keeps the stack at one height in all its branches, which is what compilers make
    of a function this small. Returns (how many bytes are on the stack at each call, how many at
    each ret), counted from the function's entry, or None if something moves the stack in a way
    this does not follow."""
    depth = 0
    frame = None
    calls, rets = [], []
    for ins in insns:
        m, ops = ins.mnemonic, ins.op_str.replace(" ", "")
        if m in ("loop", "enter"):
            return None
        if m == "push":
            depth += 4
        elif m == "pop":
            depth -= 4
        elif m in ("sub", "add") and ops.startswith("esp,"):
            try:
                n = int(ops.split(",")[1], 0)
            except ValueError:
                return None
            depth += n if m == "sub" else -n
        elif m == "mov" and ops == "ebp,esp":
            frame = depth
        elif m == "mov" and ops == "esp,ebp":
            if frame is None:
                return None
            depth = frame
        elif m == "leave":
            if frame is None:
                return None
            depth = frame - 4
        elif m == "call":
            calls.append((ins, depth))
        elif m in ("ret", "retn"):
            rets.append(depth)
        elif ops.startswith("esp,") and m not in ("cmp", "test"):
            return None
    return calls, rets


def answers_on_x87(insns):
    """Every plain return comes with a number on the floating point stack: going back from the ret,
    the first thing that touches that stack is a load, or it is the call that left its answer there."""
    for i, ins in enumerate(insns):
        if ins.mnemonic not in ("ret", "retn"):
            continue
        for back in reversed(insns[:i]):
            m = back.mnemonic
            if m == "call":
                # the stack check at the end of a function leaves the floating point stack alone
                if back.operands and back.operands[0].type == capstone.x86.X86_OP_IMM:
                    continue
                break
            if m.startswith("f"):
                if m in ("fld", "fld1", "fldz", "fild"):
                    break
                return False
        else:
            return False
    return True


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

    catch, pick, balance, boost, search, valid = [], [], [], [], [], []
    gravity, jump, jump_speed = [], [], []
    brake, kick, friction, turning, held, pad = [], [], [], [], [], []
    air_turn, spin_rate = [], []
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
        elif RAIL_SEARCH in refs:
            search.append((p, insns))
        elif RAIL_VALID in refs:
            valid.append((p, insns))
        elif AIR_GRAVITY in refs:
            gravity.append((p, insns))
        elif DO_JUMP in refs:
            jump.append((p, insns))
        elif STRUCT_GET_FLOAT in refs:
            jump_speed.append((p, insns))
        elif SHOULD_BRAKE in refs:
            brake.append((p, insns))
        elif SHOULD_KICK in refs:
            kick.append((p, insns))
        elif GROUND_FRICTION in refs:
            friction.append((p, insns))
        elif GROUND_TURN in refs:
            turning.append((p, insns))
        elif BUTTON_HELD in refs:
            held.append((p, insns))
        elif PAD_UPDATE in refs:
            pad.append((p, insns))
        elif AIR_TURN in refs:
            air_turn.append((p, insns))
        elif SCRIPTED_STAT in refs:
            spin_rate.append((p, insns))

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

    # --- rail search
    if len(search) != 1:
        problems.append("the rail search hook was found %d times" % len(search))
    for p, insns in search:
        show(dll, "rail search hook at %08X" % p, insns)
        rets = returns(insns)
        tail = insns and insns[-1].mnemonic == "jmp"
        if any(r != 0x1C for r in rets) or (not rets and not tail):
            problems.append("a rail search hook returns with %s: every return has to pop 0x1C bytes" % [hex(r) for r in rets])

    # --- rail test
    if len(valid) != 1:
        problems.append("the function that calls the game's test of a piece of rail was found %d times" % len(valid))
    for p, insns in valid:
        show(dll, "rail test at %08X" % p, insns)
        rets = returns(insns)
        if not rets or any(r != 0 for r in rets):
            problems.append("the rail test returns with %s, not with a plain ret" % rets)
        followed = stack_at_calls(insns)
        if followed is None:
            problems.append("the rail test moves the stack in a way this check does not follow")
            continue
        calls, depths = followed
        game = [(ins, depth) for ins, depth in calls if RAIL_VALID in dll.game_refs([ins]) or ins.operands[0].type == capstone.x86.X86_OP_REG]
        if len(game) != 1:
            problems.append("the rail test calls the game %d times, not once" % len(game))
        for ins, depth in game:
            print("    at the call the stack holds %d bytes more than at the entry" % depth)
            if depth < 0x20:
                problems.append("the rail test calls the game's test with %d bytes on the stack: two vectors are 32" % depth)
        # all but the saved frame pointer, if there is one, is gone again at the end
        if any(d != 0 for d in depths):
            problems.append("the rail test returns with %s bytes still on the stack" % depths)

    # --- air gravity, jump, jump speed: each stands in for a call, and has to leave the stack as the
    #     function it stands in for does
    for found, what, pops in ((gravity, "air gravity hook", 0), (jump, "jump hook", 4), (jump_speed, "jump speed hook", 0xC)):
        if len(found) != 1:
            problems.append("the %s was found %d times" % (what, len(found)))
        for p, insns in found:
            show(dll, "%s at %08X" % (what, p), insns)
            rets = returns(insns)
            tail = insns and insns[-1].mnemonic == "jmp"
            if any(r != pops for r in rets) or (not rets and not tail):
                problems.append("the %s returns with %s: every return has to pop %d bytes" % (what, [hex(r) for r in rets], pops))
            if found is gravity and not answers_on_x87(insns):
                problems.append("the air gravity hook has a return without its answer on the floating point stack")

    # --- rolling: each stands in for a call, and has to leave the stack as the function it stands in for does
    for found, what, count, pops in ((brake, "brake test hook", 1, 0), (kick, "kick test hook", 2, 0), (friction, "friction hook", 1, 0),
                                     (turning, "turning hook", 1, 0), (held, "held hook", 1, 4), (pad, "pad update hook", 1, 4),
                                     (air_turn, "air turning hook", 1, 0), (spin_rate, "spin rate hook", 1, 4)):
        if len(found) != count:
            problems.append("the %s was found %d times, not %d" % (what, len(found), count))
        for p, insns in found:
            show(dll, "%s at %08X" % (what, p), insns)
            rets = returns(insns)
            tail = insns and insns[-1].mnemonic == "jmp"
            if any(r != pops for r in rets) or (not rets and not tail):
                problems.append("the %s returns with %s: every return has to pop %d bytes" % (what, [hex(r) for r in rets], pops))
            if found is turning:
                refs = dll.game_refs(insns)
                if VECTOR_TURN not in refs or MATRIX_TURN not in refs:
                    problems.append("the turning hook does not turn the skater with the game's own functions")
            if found is spin_rate and not answers_on_x87(insns):
                problems.append("the spin rate hook has a return without its answer on the floating point stack")

    # --- the feet: the kick test hook of the ground physics, or a function it calls (three calls deep
    #     at the most), asks the game where a bone is
    feet = []
    seen = set(p for p, insns in kick)

    def walk(start, insns, depth):
        if BONE_POSITION in dll.game_refs(insns):
            feet.append((start, insns))
        if depth >= 3:
            return
        for ins in insns:
            if ins.mnemonic == "call" and ins.operands and ins.operands[0].type == capstone.x86.X86_OP_IMM:
                target = dll.entry(ins.operands[0].imm & 0xFFFFFFFF)
                if dll.in_text(target) and target not in seen:
                    seen.add(target)
                    walk(target, dll.function(target), depth + 1)

    for p, insns in kick:
        walk(p, insns, 0)
    if len(feet) != 1:
        problems.append("the code that asks the game where a bone is was found %d times, not once" % len(feet))
    for p, insns in feet:
        if any(p == k for k, _ in kick):
            print("  the feet are read in the kick test hook at %08X (above)" % p)
        else:
            show(dll, "the feet are read at %08X, which a kick test hook calls" % p, insns)
        asked = sum(1 for ins in insns if BONE_POSITION in dll.game_refs([ins]))
        calls = sum(1 for ins in insns if ins.mnemonic == "call")
        print("    it names the game's function %d times and makes %d calls" % (asked, calls))
        if calls < 2:
            problems.append("the code that reads the feet makes %d calls: it has two bones to ask about" % calls)

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
