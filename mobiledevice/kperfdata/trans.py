import sys
with open('tmp.s') as fp:
    lines = fp.read().split('\n')

def offset(line, name):
    line = line.strip()
    pos = line.find(name)
    return int(line[pos + len(name) + 1: -1], 16)

def addr(line):
    return int(line.strip().split()[0][:-1], 16)

name = lines[0].strip().strip(":")
base = addr(lines[1])

targets = set()
line2target = {}

print(name+":")
for line in lines:
    if line.strip().endswith(">") and name in line:
        target =  base + offset(line, name)
        targets.add(target)
        line2target[line] = target

#print(targets)
for line in lines[1:]:
    if not line.strip(): continue
    # print(addr(line))
    if addr(line) in targets:
        print("%s$%x:" % (name,addr(line)-base))
    if line in line2target:
        toks = line.strip().split()[1:-2]
        toks.append('%s$%x' % (name, line2target[line] - base))
        print('    ' + ' '.join(toks))
    else:
        toks = line.strip().split()[1:]
        print('    ' + ' '.join(toks))


