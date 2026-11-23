#!/usr/bin/env python3
"""THUG's script source (.q) to the game's compiled form (.qb), for Flickit's own scripts.

The source is the dialect of scripts/qb/source: SCRIPT ... ENDSCRIPT, IF / ELSE / ENDIF, BEGIN / REPEAT,
SWITCH / CASE / DEFAULT / ENDSWITCH, <param> and <...>, VECTOR(x, y, z), PAIR(x, y), RANDOM_RANGE,
"strings", #"local strings", // comments. RANDOM(...) blocks are not supported. A name that is only known
by its checksum is written $93B713A6.

A compiled file is a stream of tokens (see TOKENS): one end of line for each line that has something on it,
a name as its checksum, the names used and their text at the end (the game keeps them for its messages),
then the end of the file.

  qbc.py SOURCE.q OUT.qb            compile
  qbc.py --header SOURCE.q OUT.h    compile into a C++ header (an array of the bytes)
  qbc.py --check SOURCE.q GAME.qb   compile and compare with the game's own compiled file
"""
import re
import struct
import sys

TOKENS = {
    'end': 0x00, 'eol': 0x01, '{': 0x03, '}': 0x04, '[': 0x05, ']': 0x06, '=': 0x07, '.': 0x08, ',': 0x09,
    '-': 0x0A, '+': 0x0B, '/': 0x0C, '*': 0x0D, '(': 0x0E, ')': 0x0F, '==': 0x11, '<': 0x12, '<=': 0x13,
    '>': 0x14, '>=': 0x15, 'name': 0x16, 'int': 0x17, 'float': 0x1A, 'string': 0x1B, 'localstring': 0x1C,
    'vector': 0x1E, 'pair': 0x1F, 'BEGIN': 0x20, 'REPEAT': 0x21, 'BREAK': 0x22, 'SCRIPT': 0x23,
    'ENDSCRIPT': 0x24, 'IF': 0x25, 'ELSE': 0x26, 'ELSEIF': 0x27, 'ENDIF': 0x28, 'RETURN': 0x29,
    'checksumname': 0x2B, '<...>': 0x2C, 'arg': 0x2D, 'RANDOM_RANGE': 0x30, ':': 0x42, 'NOT': 0x39,
    '|': 0x32, '&': 0x33, 'SWITCH': 0x3C, 'ENDSWITCH': 0x3D, 'CASE': 0x3E, 'DEFAULT': 0x3F,
}
KEYWORDS = ('BEGIN', 'REPEAT', 'BREAK', 'SCRIPT', 'ENDSCRIPT', 'IF', 'ELSE', 'ELSEIF', 'ENDIF', 'RETURN',
            'RANDOM_RANGE', 'NOT', 'SWITCH', 'ENDSWITCH', 'CASE', 'DEFAULT')


def checksum(text):
    """The game's checksum of a name: CRC-32 of the lower-case text, without the final inversion."""
    crc = 0xFFFFFFFF
    for ch in text.lower().encode('latin-1'):
        crc ^= ch
        for _ in range(8):
            crc = (crc >> 1) ^ (0xEDB88320 if crc & 1 else 0)
    return crc


class Error(Exception):
    pass


_NUMBER = re.compile(r'-?(\d+\.\d*|\.\d+|\d+)')
_NAME = re.compile(r'[A-Za-z_][A-Za-z0-9_]*')
_RAW = re.compile(r'\$([0-9A-Fa-f]{8})')
_PUNCT = ('<...>', '==', '<=', '>=', '{', '}', '[', ']', '=', '.', ',', '-', '+', '/', '*', '(', ')', '<', '>',
          ':', '&', '|')


def _string(text, at, line):
    """A quoted string from text[at] (the opening quote): its bytes and where it ends."""
    out = bytearray()
    i = at + 1
    while True:
        if i >= len(text) or text[i] == '\n':
            raise Error('line %d: a string without its closing quote' % line)
        c = text[i]
        if c == '"':
            return bytes(out), i + 1
        if c == '\\' and i + 1 < len(text) and text[i + 1] in '\\"\'':
            out += text[i + 1].encode('latin-1')
            i += 2
            continue
        out += c.encode('latin-1')
        i += 1


def _floats(body, count, what, line):
    parts = [p.strip() for p in body.split(',')]
    if len(parts) != count:
        raise Error('line %d: %s needs %d numbers' % (line, what, count))
    try:
        return tuple(float(p) for p in parts)
    except ValueError:
        raise Error('line %d: %s(%s) is not numbers' % (line, what, body))


def lex(text):
    """The tokens of a source: a list of (kind, value), and the names in the order they were first used."""
    toks = []
    names = []
    seen = set()
    line = 1
    i = 0
    n = len(text)

    def add(kind, value=None):
        if kind == 'eol' and toks and toks[-1][0] == 'eol':
            return
        toks.append((kind, value))

    def name(word):
        c = checksum(word)
        if c not in seen:
            seen.add(c)
            names.append((c, word))
        add('name', c)

    while i < n:
        c = text[i]
        if c == '\n':
            add('eol')
            line += 1
            i += 1
        elif c in ' \t\r':
            i += 1
        elif text.startswith('//', i):
            while i < n and text[i] != '\n':
                i += 1
        elif c == '"':
            s, i = _string(text, i, line)
            add('string', s + b'\0')
        elif text.startswith('#"', i):
            s, i = _string(text, i + 1, line)
            add('localstring', s + b'\0')
        elif c == '<' and text.startswith('<...>', i):
            add('<...>')
            i += 5
        elif c == '<' and _NAME.match(text, i + 1) and text.find('>', i) == _NAME.match(text, i + 1).end():
            m = _NAME.match(text, i + 1)
            add('arg')
            name(m.group(0))
            i = m.end() + 1
        elif c == '$' and _RAW.match(text, i):
            m = _RAW.match(text, i)
            add('name', int(m.group(1), 16))
            i = m.end()
        elif (c.isdigit() or (c in '-.' and _NUMBER.match(text, i))) and _NUMBER.match(text, i):
            m = _NUMBER.match(text, i)
            word = m.group(0)
            if c == '-' and toks and toks[-1][0] in ('name', 'int', 'float', ')', 'arg') and text[i - 1] in ' \t':
                # "a - 1": the minus of a sum (the source writes a negative number with no space)
                pass
            if '.' in word:
                add('float', float(word))
            else:
                add('int', int(word))
            i = m.end()
        elif _NAME.match(text, i):
            m = _NAME.match(text, i)
            word = m.group(0)
            i = m.end()
            if word in ('VECTOR', 'PAIR') and i < n and text[i] == '(':
                close = text.index(')', i)
                body = text[i + 1:close]
                if word == 'VECTOR':
                    add('vector', _floats(body, 3, word, line))
                else:
                    add('pair', _floats(body, 2, word, line))
                i = close + 1
            elif word.startswith('RANDOM') and word != 'RANDOM_RANGE':
                raise Error('line %d: %s is not supported' % (line, word))
            elif word in KEYWORDS:
                add(word)
            else:
                name(word)
        else:
            for p in _PUNCT:
                if text.startswith(p, i):
                    add(p)
                    i += len(p)
                    break
            else:
                raise Error('line %d: %r is not a token' % (line, c))
    return toks, names


def check_structure(toks):
    """That every SCRIPT, IF, BEGIN, SWITCH and bracket is closed in its place."""
    stack = []
    opens = {'SCRIPT': 'ENDSCRIPT', 'IF': 'ENDIF', 'BEGIN': 'REPEAT', 'SWITCH': 'ENDSWITCH',
             '{': '}', '[': ']', '(': ')'}
    closes = {v: k for k, v in opens.items()}
    line = 1
    for kind, _ in toks:
        if kind == 'eol':
            line += 1
        elif kind in opens:
            stack.append((kind, line))
        elif kind in closes:
            if not stack or stack[-1][0] != closes[kind]:
                raise Error('around line %d: %s without its %s' % (line, kind, closes[kind]))
            stack.pop()
        elif kind in ('ELSE', 'ELSEIF') and (not stack or stack[-1][0] != 'IF'):
            raise Error('around line %d: %s outside an IF' % (line, kind))
    if stack:
        raise Error('%s from around line %d is not closed' % stack[-1])


def encode(toks, names):
    out = bytearray()
    for kind, value in toks:
        out.append(TOKENS[kind])
        if kind == 'name':
            out += struct.pack('<I', value)
        elif kind == 'int':
            out += struct.pack('<i', value)
        elif kind == 'float':
            out += struct.pack('<f', value)
        elif kind in ('string', 'localstring'):
            out += struct.pack('<I', len(value)) + value
        elif kind == 'vector':
            out += struct.pack('<3f', *value)
        elif kind == 'pair':
            out += struct.pack('<2f', *value)
    for c, word in names:
        out.append(TOKENS['checksumname'])
        out += struct.pack('<I', c) + word.encode('latin-1') + b'\0'
    out.append(TOKENS['end'])
    return bytes(out)


def compile_source(text):
    toks, names = lex(text)
    check_structure(toks)
    return encode(toks, names)


def split(data):
    """A compiled file as its code (the bytes before the table of names) and that table, as a set."""
    at = 0
    n = len(data)
    sizes = {0x16: 4, 0x17: 4, 0x18: 4, 0x19: 4, 0x1A: 4, 0x1E: 12, 0x1F: 8, 0x2E: 4, 0x02: 4}
    while at < n:
        op = data[at]
        if op == 0x2B or op == 0x00:
            break
        at += 1
        if op in sizes:
            at += sizes[op]
        elif op in (0x1B, 0x1C):
            at += 4 + struct.unpack_from('<I', data, at)[0]
        elif op in (0x2F, 0x37, 0x40, 0x41):
            count = struct.unpack_from('<I', data, at)[0]
            at += 4 + 6 * count
    code = data[:at]
    table = set()
    while at < n and data[at] == 0x2B:
        end = data.index(b'\0', at + 5)
        table.add(data[at + 1:end])
        at = end + 1
    return code, table, data[at:]


def same_code(mine, game):
    """Whether two compiled files are the same but for the order of the table of names."""
    a, ta, ea = split(mine)
    b, tb, eb = split(game)
    if a != b:
        at = next((k for k in range(min(len(a), len(b))) if a[k] != b[k]), min(len(a), len(b)))
        return False, 'the code differs at byte %d (%d / %d bytes of code)' % (at, len(a), len(b))
    if ta != tb:
        return False, 'the names differ: %d only here, %d only there' % (len(ta - tb), len(tb - ta))
    if ea != eb:
        return False, 'the ends differ'
    return True, ''


def header(data, source_name, symbol):
    lines = ['// Made by scripts/flickit/qbc.py from %s: do not change it here, change the source and' % source_name,
             '// run "python3 scripts/flickit/qbc.py --header %s src/%s.h" again.' % (source_name, symbol),
             '#pragma once',
             '#include <stdint.h>',
             '',
             'static const uint8_t %s[%d] = {' % (symbol, len(data))]
    for at in range(0, len(data), 16):
        lines.append('\t' + ' '.join('0x%02X,' % b for b in data[at:at + 16]))
    lines.append('};')
    return '\n'.join(lines) + '\n'


def main(argv):
    if len(argv) == 2 and not argv[0].startswith('--'):
        data = compile_source(open(argv[0], encoding='latin-1').read())
        open(argv[1], 'wb').write(data)
        return 0
    if len(argv) == 3 and argv[0] == '--header':
        data = compile_source(open(argv[1], encoding='latin-1').read())
        symbol = re.sub(r'\W', '_', argv[2].replace('\\', '/').split('/')[-1].rsplit('.', 1)[0])
        text = header(data, argv[1].replace('\\', '/'), symbol)
        open(argv[2], 'w', newline='\n').write(text)
        return 0
    if len(argv) == 3 and argv[0] == '--check':
        mine = compile_source(open(argv[1], encoding='latin-1').read())
        game = open(argv[2], 'rb').read()
        ok, why = same_code(mine, game)
        print('%s: %s (%d bytes)%s' % ('same' if ok else 'DIFFERENT', argv[1], len(mine), '' if ok else ': ' + why))
        return 0 if ok else 1
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
