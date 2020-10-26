_GHOOK = {}

def get_code(obj):
  func = getattr(obj, 'im_func', None)
  if func:
    return get_code(func)
  code = getattr(obj, 'func_code', None)
  if code:
    return code
  return None

def set_code(obj, code):
  func = getattr(obj, 'im_func', None)
  if func:
    return set_code(func, code)
  _code = getattr(obj, 'func_code', None)
  if _code:
    obj.func_code = code
    return True
  return False

def get_func(obj):
  func = getattr(obj, 'im_func', None)
  if func:
    return get_func(func)
  code = getattr(obj, 'func_code', None)
  if code:
    return obj
  return None
  
class DeepHook:
  def __init__(self, target, new, orig_name):
    self.target = target
    self.backup_code = get_code(target)
    #print type(target)
    self.backup = type(get_func(target))(
      target.func_code, 
      target.func_globals,
      target.func_name,
      target.func_defaults,
      target.func_closure
      )
    #print new, get_code(new)
    set_code(target, get_code(new))
    _GHOOK[orig_name] = self.backup
    
  def unhook(self):
    set_code(self.target, get_code(self.backup))
    pass

def test_function():
  def hello():
    return "hello"
  
  book = {}
  book['hello'] = hello
  
  def new_hello():
    global _GHOOK
    return _GHOOK['orig_hello']() + ' world'
    
  h = DeepHook(hello, new_hello, 'orig_hello')
  
  assert hello() == 'hello world'
  assert book['hello']() == 'hello world'
  
  h.unhook()
  
  assert hello() == 'hello'
  assert book['hello']() == 'hello'

  print "test_function pass"

def test_method():
  class Test:
    def Hello(self):
      return "hello"
  
  t = Test()
  book = {}
  book['hello'] = t.Hello
  
  def World(self):
    global _GHOOK
    return _GHOOK['Test.Hello'](self) + ' world'
  
  h = DeepHook(Test.Hello, World, 'Test.Hello')
  
  assert t.Hello() == 'hello world'
  assert book['hello']() == 'hello world'
  
  h.unhook()
  
  assert t.Hello() == 'hello'
  assert book['hello']() == 'hello'
  
  print "test_method pass"

def test_closure():
  def wrapper_gen(a, b):
    def wrapper(func):
      def ret(*l, **kw):
        return a+b+func(*l, **kw)
      return ret
    return wrapper
    
  @wrapper_gen('i','o')
  def Hello():
    return "hello"
  
  def getWorld(a,b,c):
    def World():
      global _GHOOK
      a,b,c
      return _GHOOK['hu12']() + ' world'
    return World
  book = {}
  book['hello'] = Hello
  
  assert Hello() == 'iohello'
  #print dir(Hello.func_closure[2].cell_contents)
  World = getWorld(1,2,3)
  
  h = DeepHook(Hello, World, 'hu12')
  
  #print Hello()
  
  assert Hello() == 'iohello world'
  assert book['hello']() == 'iohello world'
  
  h.unhook()
  
  assert Hello() == 'iohello'
  assert book['hello']() == 'iohello'
  
  print "test_closure pass"

def test_arguments():
  def Hello(a,b,c = 3):
    return "%s %s %s" % (a,b,c)
  
  def World(a,b,c = 3):
    global _GHOOK
    return _GHOOK['test_arg'](a,b,c) + ' 666'
  
  h = DeepHook(Hello, World, 'test_arg')
  
  assert Hello(7, 8) == '7 8 3 666'
  
  h.unhook()
  
  assert Hello(1, 2) == '1 2 3'
  
  print  "test_arguments pass"
  

if __name__ == '__main__':
  test_function()
  test_method()
  test_closure()
  test_arguments()
  pass