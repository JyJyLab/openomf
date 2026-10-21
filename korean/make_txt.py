#!/usr/bin/env python3
"""Merge the translation files (korean/tr/*.py, each defines TR = {id: text}) into KOREAN.TXT.
Entries without a translation keep the English text. Prints how many entries are translated."""
import glob
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))


def parse(path):
    t = open(path, encoding='utf-8').read()
    return [(int(i), ti, d) for i, ti, d in
            re.findall(r'ID: (\d+)\nTitle: ([^\n]*)\nData: (.*?)(?=\n+ID: \d+\nTitle: |\n*\Z)', t, re.S)]


def main():
    tr = {}
    for f in sorted(glob.glob(os.path.join(HERE, 'tr', '*.py'))):
        ns = {}
        exec(open(f, encoding='utf-8').read(), ns)
        tr.update(ns['TR'])
    ents = parse(os.path.join(HERE, 'ENGLISH-source.txt'))
    done = 0
    with open(os.path.join(HERE, 'KOREAN.TXT'), 'w', encoding='utf-8') as out:
        for i, ti, d in ents:
            if i in tr:
                d = tr[i]
                done += 1
            out.write('ID: %d\nTitle: %s\nData: %s\n\n' % (i, ti, d))
    print('KOREAN.TXT: %d / %d entries translated' % (done, len(ents)))


if __name__ == '__main__':
    main()
