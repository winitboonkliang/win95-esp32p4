"""B82: move every writable, initialized FILE-SCOPE variable of the Doom engine out of internal RAM (.data) - lead rule: static internal
delta <= ~300 B. For each `SPEC DECL = INIT;` at file scope (not const, not extern/typedef):
    SPEC <DECL with `const` before the name, name -> name__b82i> = INIT;     -> flash rodata (the initial value)
    SPEC DECL;                                                              -> .bss -> PSRAM (components/doomp4/linker.lf)
and one `void b82i_<file>(void)` per file copies the values at engine start (b82_data_init, generated into hooks/b82_datainit.c).
Pointers inside the initializers keep pointing at the live (PSRAM) objects. Function-scope statics are left alone (listed).
usage: python b82_data2psram.py <out_init_c> <file.c>...   (in place, idempotent: files marked B82_DATA2PSRAM are skipped)"""
import os, re, sys

KEYW = {"typedef", "extern"}


def chunks(src):
    """top-level statements: (start, end, kind) kind = 'decl' (ends with ';' at depth 0) | 'other'"""
    out = []; i = 0; n = len(src); depth = 0; start = 0; line_start = True
    while i < n:
        c = src[i]
        if line_start and depth == 0 and c == '#':            # preprocessor line (with continuations): its own chunk
            j = i
            while True:
                e = src.find('\n', j)
                if e < 0: e = n; break
                if src[e - 1] == '\\': j = e + 1; continue
                break
            if src[start:i].strip(): out.append((start, i, 'other'))
            out.append((i, e, 'pp')); i = e; start = e; continue
        if c == '\n': line_start = True; i += 1; continue
        if not c.isspace(): line_start = False
        if src.startswith('//', i): e = src.find('\n', i); i = n if e < 0 else e; continue
        if src.startswith('/*', i): e = src.find('*/', i + 2); i = n if e < 0 else e + 2; continue
        if c in '"\'':
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == '\\' else 1
            i = j + 1; continue
        if c in '{([': depth += 1
        elif c in '})]':
            depth -= 1
            if depth == 0 and c == '}':
                k = i + 1                                        # function body or struct/enum definition ends here (unless ';' follows)
                while k < n and src[k] in ' \t\r\n': k += 1
                head = strip_comments(src[start:i]).lstrip()
                if k < n and src[k] == ';': i = k
                elif re.match(r'(static\s+|const\s+|typedef\s+)*(struct|union|enum)', head): i += 1; continue   # struct body, declarator follows
                else:
                    out.append((start, i + 1, 'other')); start = i + 1; i += 1; continue
                out.append((start, i + 1, 'decl')); start = i + 1; i += 1; continue
        elif c == ';' and depth == 0:
            out.append((start, i + 1, 'decl')); start = i + 1
        i += 1
    if start < n: out.append((start, n, 'other'))
    return out


def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', ' ', s, flags=re.S); return re.sub(r'//[^\n]*', ' ', s)


def top_eq(s):
    """index of the first '=' at depth 0 (not ==), -1 if none"""
    d = 0; i = 0
    while i < len(s):
        c = s[i]
        if c in '"\'':
            j = i + 1
            while j < len(s) and s[j] != c: j += 2 if s[j] == '\\' else 1
            i = j + 1; continue
        if c in '{([': d += 1
        elif c in '})]': d -= 1
        elif c == '=' and d == 0 and s[i + 1:i + 2] != '=' and s[i - 1:i] not in '!<>=': return i
        elif c == ',' and d == 0: return -2   # several declarators: not handled
        i += 1
    return -1


def transform(path):
    src = open(path, 'rb').read().decode('latin-1')
    if 'B82_DATA2PSRAM' in src: return None, []
    nl = '\r\n' if '\r\n' in src else '\n'
    base = re.sub(r'\W', '_', os.path.splitext(os.path.basename(path))[0])
    out = []; pos = 0; moved = []; skipped = []
    ifd = 0
    for a, b, kind in chunks(src):
        if kind == 'pp':
            d = src[a:b].lstrip('# 	')
            if d.startswith('if'): ifd += 1
            elif d.startswith('endif'): ifd -= 1
            continue
        if kind != 'decl': continue
        if ifd > 0:   # conditional code: the init function would name objects that may not exist
            if '=' in src[a:b]: skipped.append('(#if) ' + strip_comments(src[a:b]).strip()[:70])
            continue
        text = src[a:b]; code = strip_comments(text).strip()
        if not code.endswith(';') or '=' not in code: continue
        first = code.split()[0]
        if first in KEYW: continue
        k = top_eq(code)
        if k == -2: skipped.append(code[:80]); continue
        if k < 0: continue
        lhs = code[:k].rstrip(); init = code[k + 1:-1].strip()
        if '(' in lhs: skipped.append(code[:80]); continue          # function pointers / complex declarators: by hand
        m = re.match(r'^(.*?)([A-Za-z_]\w*)\s*((?:\[[^\]]*\]\s*)*)$', lhs, re.S)
        if not m: skipped.append(code[:80]); continue
        spec_decl, name, dims = m.group(1), m.group(2), m.group(3).strip()
        if '{' in spec_decl: skipped.append('(anon type) ' + code[:70]); continue
        if not spec_decl.strip(): continue   # tail of a `struct {...} name = ...` split by the chunker: by hand (make it const)
        stars = spec_decl.rstrip()
        is_const = (re.search(r'\bconst\b', stars) and '*' not in stars) or re.search(r'\*\s*const\s*$', stars)
        if is_const: continue
        static = 'static ' if re.search(r'\bstatic\b', spec_decl) else ''
        live_dims = dims
        if '[]' in dims.replace(' ', ''):
            live_dims = re.sub(r'\[\s*\]', '[sizeof %s__b82i / sizeof %s__b82i[0]]' % (name, name), dims, count=1)
        new = ('%s const %s__b82i%s = %s;  /* B82: initial value in flash */%s%s%s%s;  /* B82: live copy, .bss -> PSRAM */'
               % (spec_decl.rstrip() if spec_decl.strip() else '', name, dims, init, nl, spec_decl, name, live_dims))
        if not new.lstrip().startswith(('static', 'const')) and static == '' and not spec_decl.strip():
            pass
        # the init copy is always file-local
        new = ('static ' if not static else '') + new
        out.append((a, b, new)); moved.append(name)
    if not moved: return src, skipped
    res = []; last = 0
    for a, b, new in out:
        res.append(src[last:a]); lead = src[a:b][:len(src[a:b]) - len(src[a:b].lstrip())]; res.append(lead + new); last = b
    res.append(src[last:])
    s = ''.join(res)
    s += nl + '/* B82_DATA2PSRAM: generated by lab:LEAD/B82/tools/b82_data2psram.py */' + nl + '#include <string.h>' + nl
    s += 'void b82i_%s(void)%s{%s' % (base, nl, nl)
    for nm in moved: s += '    memcpy((void *)&%s, (const void *)&%s__b82i, sizeof %s__b82i);%s' % (nm, nm, nm, nl)
    s += '}' + nl
    return s, skipped, (base, moved)


if __name__ == '__main__':
    out_c = sys.argv[1]; bases = []
    for p in sys.argv[2:]:
        r = transform(p)
        if r[0] is None: print('skip (done)', p); continue
        s, skipped = r[0], r[1]
        if len(r) == 3:
            open(p, 'wb').write(s.encode('latin-1')); bases.append(r[2][0]); print('%-16s moved %3d  %s' % (os.path.basename(p), len(r[2][1]), ' '.join(r[2][1])[:150]))
        for k in skipped: print('    BY HAND:', os.path.basename(p), k.replace('\n', ' '))
    if bases:
        g = '/* B82: generated by tools/b82_data2psram.py - initial values of the engine globals, flash -> PSRAM, once before D_DoomMain */\n'
        g += ''.join('void b82i_%s(void);\n' % b for b in bases) + 'void b82_data_init(void)\n{\n' + ''.join('    b82i_%s();\n' % b for b in bases) + '}\n'
        open(out_c, 'w').write(g)
