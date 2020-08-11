import asyncio
import sys
import plistlib
import struct
from collections import namedtuple
import itertools
import time
from ctypes import sizeof
from dtxlib import DTXMessage, DTXMessageHeader,    \
    auxiliary_to_pyobject, pyobject_to_auxiliary,   \
    pyobject_to_selector, selector_to_pyobject
    
COUNTER = itertools.count()

from ctypes import c_uint32, c_uint16, Structure

def now():
  return int(round(time.time() * 1000))

def hexdump(buf):
  allowed = '_qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890+-_=()*&^%$#@![]{}\\|;\':"<>?,./`~'
  def form1(b):
    return ' '.join(map(lambda x:'%02x' % x, b))
  def form2(b):
    return ''.join(map(lambda c: c if c in allowed else '.', b.decode('ascii', "replace")))
  LINE_BYTES = 32
  lines = [buf[i: i + LINE_BYTES] for i in range(0, len(buf), LINE_BYTES)]
  for l in lines:
    print(form1(l), form2(l))

def hexdump2(buf, prefix=''):
  if not buf: return prefix + '<None>\n'
  allowed = '_qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890+-_=()*&^%$#@![]{}\\|;\':"<>?,./`~'
  def form1(b):
    return ' '.join(map(lambda x:'%02x' % x, b))
  def form2(b):
    return ''.join(map(lambda c: c if c in allowed else '.', b.decode('ascii', "replace")))
  LINE_BYTES = 32
  lines = [buf[i: i + LINE_BYTES] for i in range(0, len(buf), LINE_BYTES)]
  ret = []
  for l in lines:
    ret += [prefix, form1(l), ' ', form2(l), '\n']
  return ''.join(ret)
  
class UsbmuxHeader(Structure):
  _fields_ = [
    ('length', c_uint32),
    ('reserved', c_uint32),
    ('type', c_uint32),
    ('tag', c_uint32)
  ]

class UsbmuxResult(Structure):
  _fields_ = [
    ('header', UsbmuxHeader),
    ('result', c_uint32)
  ]
  
class UsbmuxConnectionRequest(Structure):
  _fields_ = [
    ('header', UsbmuxHeader),
    ('device_id', c_uint32),
    ('port', c_uint16)
  ]

USBMUX_RESULT  = 1
USBMUX_CONNECT = 2
USBMUX_HELLO   = 3
USBMUX_UDID   = 3
USBMUX_PLIST = 8


USBMUX_STATE_USBMUX = 1
USBMUX_STATE_LOCKDOWN = 2
USBMUX_STATE_OTHER = 3

class UsbmuxDecoder:

  class HeaderDecoder:
  
    def __init__(self):
      self._buf = b''
      
    def update(self, buf):
      p = buf[:16 - len(self._buf)]
      self._buf += p
      if len(self._buf) == 16:
        # complete
        hdr = UsbmuxHeader.from_buffer_copy(self._buf)
        return BodyDecoder(hdr), len(p)
      return self, len(p)

  class BodyDecoder:
    
    def __init__(self, header):
      self.hdr = header
      self._buf = b''
      self._size = self.hdr.length - 16
    
    def update(self, buf):
      p = buf[:self._size - len(self._buf)]
      self._buf += p
      if len(self._buf) == self._size:
        #complete
        return HeaderDecoder(), len(p)
      return self, len(p)

  def __init__(self, tag, state):
    self.free_mode = False
    self._reset()
    self.TAG = tag
    self.state = state
    
  def _reset(self):
    self.hdr = b''
    self.body_size = 0
    self.body = b''
    
  def _update(self, buf):
    if self._state_header():
      return self._update_header(buf)
    if self._state_body():
      return self._update_body(buf)

  def update(self, buf):
    # self.state.log(self.TAG, buf)
    while buf and self.state.state == USBMUX_STATE_USBMUX:
      n = self._update(buf)
      buf = buf[n:]
    if buf:
      self.state.handle_real_packet(self.TAG, buf)

  def _state_header(self):
    return len(self.hdr) < 16
    
  def _state_body(self):
    return self.body_size > len(self.body)

  def _update_header(self, buf):
    p = buf[:16 - len(self.hdr)]
    self.hdr += p
    if len(self.hdr) == 16:
      hdr = UsbmuxHeader.from_buffer_copy(self.hdr)
      self.body_size = hdr.length - 16
      if self.body_size == 0:
        self._complete_packet()
    return len(p)
  
  def _update_body(self, buf):
    p = buf[:self.body_size - len(self.body)]
    self.body += p
    if len(self.body) == self.body_size:
      self._complete_packet()
    return len(p)
    
  def _complete_packet(self):
    self.state.handle_packet(self.TAG, self.hdr + self.body)
    self._reset()
  
USBMUX_SUBSTATE_LIST_DEVICES = 1
USBMUX_SUBSTATE_CONNECT = 2
USBMUX_SUBSTATE_PAIR_RECORD = 3
USBMUX_SUBSTATE_LISTEN = 4
USBMUX_SUBSTATE_READ_BUID = 5

def real_port_number(num):
  return ((num & 0xff) << 8) + ((num & 0xff00) >> 8)

TLS_RECORD_NAME = {
  b'\x14': 'CHANGE_CIPHER_SPEC',
  b'\x15': 'ALERT',
  b'\x16': 'HANDSHAKE',
  b'\x17': 'APPLICATION_DATA',
}

TLS_HANDSHAKE_NAME = {
  b'\x00': 'HELLO_REQUEST',
  b'\x01': 'CLIENT_HELLO',
  b'\x02': 'SERVER_HELLO',
  b'\x0b': 'CERTIFICATE',
  b'\x0c': 'SERVER_KEY_EXCHANGE',
  b'\x0d': 'CERTIFICATE_REQUEST',
  b'\x0e': 'SERVER_DONE',
  b'\x0f': 'CERTIFICATE_VERIFY',
  b'\x10': 'CLIENT_KEY_EXCHANGE',
  b'\x14': 'FINISHED',
}
class TLSDecoder:
  def __init__(self, tag, conn):
    self.tag = tag
    self.conn = conn
    self._reset()
    
  def _reset(self):
    self.hdr = b''
    self.body_size = 0
    self.body = b''
  
  def _state_header(self):
    return len(self.hdr) < 5
    
  def _state_body(self):
    return self.body_size > len(self.body)
    
  def _update(self, buf):
    if self._state_header():
      return self._update_header(buf)
    if self._state_body():
      return self._update_body(buf)
      
  def update(self, buf):
    while buf:
      n = self._update(buf)
      buf = buf[n:]
  
  def _update_header(self, buf):
    p = buf[:5 - len(self.hdr)]
    self.hdr += p
    if len(self.hdr) == 5:
      record_type, version_major, version_minor, record_length = struct.unpack(">BBBH", self.hdr)
      self.body_size = record_length
      if self.body_size == 0:
        self._complete_packet()
    return len(p)
  
  def _update_body(self, buf):
    p = buf[:self.body_size - len(self.body)]
    self.body += p
    if len(self.body) == self.body_size:
      self._complete_packet()
    return len(p)
    
  def _complete_packet(self):
    msg = self.hdr + self.body
    record_type = TLS_RECORD_NAME.get(msg[:1], 'WTF?')
    if msg[:1] == b'\x16':
      record_type += '-' + TLS_HANDSHAKE_NAME.get(msg[5:6], 'ENCRYPTED?')
    more = ""
    if len(msg) > 24: 
      more = "..."
    #print(f"\n{self.tag} {now()} [lockdown TLS {record_type} tag={self.conn}] {msg.hex()[:48]}{more}") # not a typo here, too many `tag`s, fuck! 
    print(f"\n{self.tag} {now()} [lockdown TLS {record_type} tag={self.conn}]") # not a typo here, too many `tag`s, fuck! 
    hexdump(msg)
    
    self._reset()


class RawArrayDecoder:
  
  def __init__(self, expected):
    self.expected = expected
    self._buf = []
    print("RawArrayDecoder ", expected)

  def update(self, buf):
    to_consume = min(len(buf), self.expected)
    if to_consume:
      self.expected -= to_consume
      self._buf.append(buf[:to_consume])
    if self.expected == 0:
      return None, to_consume
    return self, to_consume
  
  @property
  def data(self):
    return b''.join(self._buf)

class DTXHeaderDecoder:

  def __init__(self):
    self._d = RawArrayDecoder(sizeof(DTXMessageHeader))
    self._hdr = None
    
  def update(self, buf):
    d, p = self._d, 0
    while d is not None and p < len(buf):
      d, p_ = self._d.update(buf[p:])
      p += p_
    
    if not d:
      self._hdr = DTXMessageHeader.from_buffer_copy(self._d.data)
      return None, p
    return self, p
  
  @property
  def data(self):
    return self._hdr

 

class FragmentDecoder:
  
  def __init__(self):
    self._d = DTXHeaderDecoder()
    self._hdr = None
    self._body = None
    
  def update(self, buf):
    d, p = self._d, 0
    while d is not None and p < len(buf):
      d, p_ = self._d.update(buf[p:])
      p += p_
      
    if not d:
      if not self._hdr:
        self._hdr = self._d.data
        if self._hdr.fragmentCount > 1 and self._hdr.fragmentId == 0:
          return None, p
        else:
          self._d = RawArrayDecoder(self._hdr.length)
          return self, p
      else:
        self._body = self._d.data
        return None, p
    return self, p
  
  @property
  def data(self):
    return (self._hdr, self._body)
    """
    if self._hdr.fragmentCount == 1 and self._hdr.fragmentId == 0:
      try:
        dtx = DTXMessage.from_bytes(bytes(self._hdr) + self._body)
        sel = dtx.get_selector()
        if sel.startswith(b'bplist00'):
          sel = selector_to_pyobject(sel)
        c = dtx.get_auxiliary_count()
        return (sel, c, [auxiliary_to_pyobject(dtx.get_auxiliary_at(i)) for i in range(c)])
      except:
        return f"<decode error>\n " + hexdump2(self._body)
    return (self._hdr.magic, f'{self._hdr.fragmentId}/{self._hdr.fragmentCount}', f'{self._hdr.channelCode}-{self._hdr.identifier}', hexdump2(self._body))
    """
    
class DTXDecoder:
  
  def __init__(self, tag, conn):
    self.tag = tag
    self.conn = conn
    self._d = FragmentDecoder()
    self._fp = open(f"dtx-{id(self)}.log", "w")
    self._fp_bin = open(f"dtx-{id(self)}.log.bin", "wb")
    self._fragment_tracker = {}
    
  def update(self, buf):
    self._fp_bin.write(buf)
    d = self._d
    while buf:
      d, p = self._d.update(buf)
      if d == None:
        # print(self._d.data)
        # self._fp.write(f'{self._d.data}\n')
        # self.log(*self._d.data)
        self._d = FragmentDecoder()
      buf = buf[p:]
  
  def log_dtx_msg(self, buf):
    try:
      dtx = DTXMessage.from_bytes(buf)
      sel = dtx.get_selector()
      if sel.startswith(b'bplist00'):
        sel = selector_to_pyobject(sel)
        c = dtx.get_auxiliary_count()
      self._fp.write(f"\tsel={sel}\n")
      if c:
        for i in range(c):
          self._fp.write(f"\t\targ{i}={auxiliary_to_pyobject(dtx.get_auxiliary_at(i))}\n")
    except:
      self._fp.write(f"\t! decode error !\n")
      self._fp.write(hexdump2(buf, '\t'))

  def log(self, hdr, body):
    k = f'<{hdr.channelCode}-{hdr.identifier}>'
    self._fp.write(f"<{hdr.channelCode}-{hdr.identifier}> <{hdr.fragmentId}/{hdr.fragmentCount}> magic={hdr.magic}\n")
    if hdr.fragmentCount == 1 and hdr.fragmentId == 0:
      self.log_dtx_msg(bytes(hdr) + body)
    else:
      if hdr.fragmentCount > 1:
        if hdr.fragmentId == 0:
          self._fragment_tracker[k] = [bytes(hdr)]
        else:
          self._fragment_tracker[k] += [bytes(hdr), body]
        if hdr.fragmentId == hdr.fragmentCount - 1:
          self.log_dtx_msg(b''.join(self._fragment_tracker[k]))
          self._fragment_tracker.pop(k)
      else:
        assert False, "WTF?"
      #self._fp.write(hexdump2(body, '\t'))
    

class LockdownDecoder:
  
  def __init__(self, tag, conn):
    self.tag = tag
    self.conn = conn
    self._reset()
    self.hijack = None
    self.giveup = False
    
  def _reset(self):
    self.hdr = b''
    self.body_size = 0
    self.body = b''
    
  def _update(self, buf):
    if self._state_give_up():
      return self._update_giveup(buf)
    if self._state_hijack():
      return self._update_hijack(buf)
    if self._state_header():
      return self._update_header(buf)
    if self._state_body():
      return self._update_body(buf)
      
  def _state_header(self):
    return len(self.hdr) < 4
    
  def _state_body(self):
    return self.body_size > len(self.body)
  
  def _state_give_up(self):
    return self.giveup
  
  def _state_hijack(self):
    return self.hijack is not None
    
  def update(self, buf):
    while buf:
      n = self._update(buf)
      buf = buf[n:]
    pass
  
  def _update_giveup(self, buf):
    p = len(buf)
    if self.body or self.hdr:
      buf = self.hdr + self.body + buf
      self._reset()
    print(f"\n{self.tag} {now()} [lockdown tag={self.conn}]") # not a typo here, too many `tag`s, fuck! 
    print(f"\tRAW_DATA: {buf}")
    return p
  
  def _update_hijack(self, buf):
    p = len(buf)
    self.hijack.update(buf)
    return p

  def _update_header(self, buf):
    p = buf[:4 - len(self.hdr)]
    self.hdr += p
    if len(self.hdr) == 4:
      self.body_size, = struct.unpack(">I", self.hdr)
      if self.body_size == 0:
        self._complete_packet()
    return len(p)
  
  def _update_body(self, buf):
    p = buf[:self.body_size - len(self.body)]
    self.body += p
    if len(self.body) == self.body_size:
      self._complete_packet()
    if len(self.body) >= 8 and not (self.body[:8] == b'bplist00' or self.body[:8] == b'<?xml ve'):
      msg = self.hdr + self.body
      if msg[:3] == b'\x16\x03\x01' or msg[:3] == b'\x16\x03\x03':
        self.hijack = TLSDecoder(self.tag, self.conn)
        self.hijack.update(msg)
        self._reset()
      elif msg[:4] == b'\x79\x5b\x3d\x1f':
        self.hijack = DTXDecoder(self.tag, self.conn)
        self.hijack.update(msg)
        self._reset()
      else:
        self.giveup = True
    return len(p)
    
  def _complete_packet(self):
    print(f"\n{self.tag} {now()} [lockdown tag={self.conn}]") # not a typo here, too many `tag`s, fuck! 
    try:
      msg = plistlib.loads(self.body)
      print(f"\tPLIST: {msg}")
    except plistlib.InvalidFileException:
      print(f"\tPLIST_DECODE_FAILED: {self.body}")
    
    self._reset()

def plist_show_pair_record_data(msg):
  print("\tPAIR_RECORD:", plistlib.loads(msg['PairRecordData']))
  
def plist_show_default(msg):
  print("\tPLIST:", msg)
  
class UsbmuxState:

  def __init__(self, fp):
    self.fp = fp
    self.state = USBMUX_STATE_USBMUX
    self.sub_state = 0
    self.extra = None
    self.lockdown = {}
    self.connection_id = None

  def log(self, tag, buf):
    msg = f'{tag}[{len(buf)}]'.encode('utf8') + buf
    self.fp.write(msg)
  
  def _handle_plist_packet(self, tag, hdr, data):
    print(f"\n{tag} {now()}: <length={hdr.length} reserved={hdr.reserved} type={hdr.type} tag={hdr.tag}>")
    msg = plistlib.loads(data)
    plist_display_func = plist_show_default
    cap = f'\n{tag}[plist]:\n{msg}'.encode('utf8')
    self.fp.write(cap)
    
    if self.sub_state:
      if self.sub_state == USBMUX_SUBSTATE_LIST_DEVICES:
        self.sub_state = 0
        
      elif self.sub_state == USBMUX_SUBSTATE_CONNECT:
        if msg['MessageType'] == 'Result':
          if msg['Number'] == 0:
            self.connection_id = hdr.tag
            if self.extra == 32498: # big endian lockdownd port 62078 in little endian
              print("\tConnection to lockdownd made!")
              self.state = USBMUX_STATE_LOCKDOWN
            else:
              print("\tConnection to %d made!" % real_port_number(self.extra))
              self.state = USBMUX_STATE_LOCKDOWN
          else:
            print("\tConnection Error: ", msg['Number'])
        else:
          print("**********************************************")
          print("[!] UNKOWN MessageType for connecting:", msg['MessageType'])
          print("**********************************************")
        self.sub_state = 0
        self.extra = None
        
      elif self.sub_state == USBMUX_SUBSTATE_PAIR_RECORD:
        self.sub_state = 0
        plist_display_func = plist_show_pair_record_data
      elif self.sub_state == USBMUX_SUBSTATE_LISTEN:
        self.sub_state = 0
        
      elif self.sub_state == USBMUX_SUBSTATE_READ_BUID:
        self.sub_state = 0
        
      else:
        print("**********************************************")
        print("[!] UNKOWN SUBSTATE:", self.sub_state)
        print("**********************************************")
    elif msg['MessageType'] == 'ListDevices':
      self.sub_state = USBMUX_SUBSTATE_LIST_DEVICES
    elif msg['MessageType'] == 'Listen':
      self.sub_state = USBMUX_SUBSTATE_LISTEN
    elif msg['MessageType'] == 'Attached':
      pass
    elif msg['MessageType'] == 'Detached':
      pass
    elif msg['MessageType'] == 'Connect':
      print("\tConnecting to remote port:", real_port_number(msg['PortNumber']))
      self.sub_state = USBMUX_SUBSTATE_CONNECT
      self.extra = msg['PortNumber']
      pass
    elif msg['MessageType'] == 'ReadPairRecord':
      self.sub_state = USBMUX_SUBSTATE_PAIR_RECORD
    elif msg['MessageType'] == 'ReadBUID':
      self.sub_state = USBMUX_SUBSTATE_READ_BUID
    else:
      print("**********************************************")
      print("[!] UNKOWN MESSAGE TYPE:", msg['MessageType'])
      print("**********************************************")
    plist_display_func(msg)

  def handle_packet(self, tag, packet):
    hdr = UsbmuxHeader.from_buffer_copy(packet[:16])
    if hdr.type == USBMUX_PLIST:
      self._handle_plist_packet(tag, hdr, packet[16:])
    else:
      print(f"\n{tag} {now()}: <length={hdr.length} reserved={hdr.reserved} type={hdr.type} tag={hdr.tag}>")
      print("\t", packet[16:].decode("utf-8"))
    pass
  
  def _get_lockdown_decoder(self, tag):
    if tag in self.lockdown:
      return self.lockdown[tag]
    ret = LockdownDecoder(tag, self.connection_id)
    self.lockdown[tag] = ret
    return ret

  def handle_real_packet(self, tag, buf):
    msg = f'\n{tag}[{len(buf)}]:\n'.encode('utf8') + buf
    self.fp.write(msg)
    if self.state == USBMUX_STATE_LOCKDOWN:
      self._get_lockdown_decoder(tag).update(buf)

class AutoIncrementCounter:
  def __init__(self):
    self._idx = 0
  @property
  def index(self):
    ret = self._idx
    self._idx += 1
    return self._idx
    
async def client_connected(s_rd, s_wr):
  c_rd, c_wr = await asyncio.open_connection(host="127.0.0.1", port=27015)
  idx = next(COUNTER)
  fp = open(f'packet_{idx}', 'wb')
  state = UsbmuxState(fp)
  async def t(rd, wr, wr2, tag):
    decoder = UsbmuxDecoder(tag, state)
    while 1:
      b = await rd.read(1024)
      if not b:
        break
      decoder.update(b)
      wr.write(b)
    wr.close()
    wr2.close()
    print("[-] connection closed...", file=sys.stderr)
  await asyncio.gather(t(s_rd, c_wr, s_wr, "xcode->iphone"), t(c_rd, s_wr, c_wr, "iphone->xcode"))
  fp.close()

async def main(loop, host, port):
  server = await asyncio.start_server(client_connected, host, port, start_serving=True)
  await server.serve_forever()

async def keyboardinterrupt():
  while True:
    await asyncio.sleep(1)

async def cancel_all_tasks(loop):
  for task in asyncio.all_tasks():
    if task is not asyncio.current_task():
      task.cancel()
  
if __name__ == '__main__':
  loop = asyncio.get_event_loop()
  loop.create_task(keyboardinterrupt()),  # hack until python 3.8
  loop.create_task(main(loop, '127.0.0.1', 27016))
  try:
    loop.run_forever()
  except KeyboardInterrupt as e:
    print("Caught keyboard interrupt. Canceling tasks...")
    loop.stop()
    loop.run_until_complete(cancel_all_tasks(loop))
    loop.run_until_complete(loop.shutdown_asyncgens())
  finally:
    loop.close()
