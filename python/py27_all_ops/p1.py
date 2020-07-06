import os

from os import path
from os import *
def gen():
  for i in range(100)[::-1]:
    yield -i

def my_min(a,b,c):
  return min(b, c, a)

def mk_clos():
  x  = {}
  y = x
  def f():
    b = y[22] = 33
    return b.items();
  return f

def ll(*l, **kw):
  return len(globals())

for i in gen():
  print i
  if i == 78:
    break
  else:
    continue
  print "no"

a = mk_clos()
+6
-5
4*ll()
not True

a() or a()
exec "print 1"
try:
  1/0
except:
  pass
finally:
  pass

a = []
a.append("111")
a += [12,3,4]
d = (a, a)
a = set(range(10))
b = set("hello")
a.add(11)

a = {}
a &= a
a |= a
a ^= a
ll(788, bb=11)
a = a & a | a ^ a << 1 >> 1 ** 2 % 88 + 22 - 77 / 11 // 1.1
a **= 3
a //= 4.5
class a():
  l = 1
  pass
aa = a
aa.l = 2
del aa.l

with a():
  pass

i = 0
s = 0
while s < 1000:
  if i % 3 == 0:
    continue
  s += i
  
a = "123"
b = a + a
b = list(b * 10)

b[4:5] = [1]
b[:5] = [1,2,4]
b[8:] = [1,3,5]
b[:] = list('a' * 100)
b[5] = 0

for i in b:
  continue

t = b[4:5]
t = b[:6]
t = b[7:]
t = b[:]

del b[1:2]
del b[:3]
del b[10:]
del b[::2]
del b[:]

for i in range(100)[2::3]:
  print i

for i in range(100)[2:88]:
  print i

y = 55
y *= 3
y /= 6
y %= 9
y -= 8
y <<= 1
y >>= 1
y = ~y
y = `y`
y = -y

z = 1.0
z /= 8.0
z = z ** 9
z = z | z
z = z & z
z = z ^ z

del z
ll(*(1,2,3), **{"K":88})

a,b,c=1,2,3

def aa(a1=88, a2=99):
  b = 99.0
  global a
  a = 1
  del a 
  c = b / 1.0
  del b
  pass
  
aa(a2=88)

xx = [2*i for i in range(10)]
xx[1] += 1

{1,2} # BUILD_SET
{i for i in range(10)} # SET_ADD
{i:i for i in range(10)} # MAP_ADD

# CONTINUE_LOOP
while False: 
  try:
    continue
  except:
    pass

y[1:2] += [1] #ROT_FOUR    

def fun_kw(a, **kw):
  print a, str(kw)

def fun_var(*l):
  print str(l)

print >> sys.stderr, 'spam'

raise NotImplementedError

func_kw(8, **{"a":3,"b":4})
fun_var(*[1,2,3])

if not xx:
    pass
else:
    pass
    
def _removeHandlerRef(wr):
    acquire, release, handlers = _acquireLock, _releaseLock, _handlerList
    if acquire and release and handlers:
        acquire()
        try:
            if wr in handlers:
                handlers.remove(wr)
        finally:
            release()
            
a and b

def isname(name):
  # check that group name is a valid string
  if not isident(name[0]):
      return False
  for char in name[1:]:
      if not isident(char) and not isdigit(char):
          return False
  return True