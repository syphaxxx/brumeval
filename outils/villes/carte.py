"""Aide pour modifier les cartes de Brumeval en gardant le format exact de writeJson (store.cpp)."""
import json

R = 'C:/Users/nassim/code/brumeval/data/'


def scalar(v):
    if isinstance(v, float):
        if abs(v - round(v)) < 1e-9:
            return f'{int(round(v))}.0'
        s = f'{v:.4f}'.rstrip('0')
        return s + '0' if s.endswith('.') else s
    return json.dumps(v, ensure_ascii=False)


def inline(j):
    if isinstance(j, dict):
        return '{}' if not j else '{' + ', '.join(json.dumps(k, ensure_ascii=False) + ': ' + inline(v) for k, v in j.items()) + '}'
    if isinstance(j, list):
        return '[]' if not j else '[' + ', '.join(inline(v) for v in j) + ']'
    return scalar(j)


def pretty(j, ind=0, used=0):
    flat = inline(j)
    if not isinstance(j, (dict, list)) or not j or (ind > 0 and used + len(flat.encode('utf-8')) <= 110):
        return flat
    out = '{\n' if isinstance(j, dict) else '[\n'
    items = list(j.items()) if isinstance(j, dict) else [(None, v) for v in j]
    for i, (k, v) in enumerate(items):
        out += ' ' * (ind + 2)
        u = ind + 2
        if k is not None:
            ks = json.dumps(k, ensure_ascii=False) + ': '
            out += ks
            u += len(ks.encode('utf-8'))
        out += pretty(v, ind + 2, u)
        if i + 1 < len(items):
            out += ','
        out += '\n'
    return out + ' ' * ind + ('}' if isinstance(j, dict) else ']')


def load(name, sub='cartes/'):
    return json.load(open(R + sub + name + '.json', encoding='utf-8'))


def save(name, m, sub='cartes/'):
    open(R + sub + name + '.json', 'w', encoding='utf-8', newline='\n').write(pretty(m) + '\n')


class Grid:
    def __init__(self, rows):
        self.g = [list(r) for r in rows]

    def put(self, x, y, c):
        self.g[y][x] = c

    def rect(self, x0, y0, x1, y1, c):  # bornes comprises
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.g[y][x] = c

    def text(self, x, y, s):  # une ligne de caractères à partir de (x, y) ; ' ' = ne change pas
        for i, c in enumerate(s):
            if c != ' ':
                self.g[y][x + i] = c

    def rows(self):
        return [''.join(r) for r in self.g]


def roundtrip_ok(name):
    p = R + 'cartes/' + name + '.json'
    s = open(p, encoding='utf-8').read()
    return pretty(json.loads(s)) + '\n' == s
