__all__ = [
    'iter_only', 'bitflag', 'msb_fromstring', 'msb_toint',
    'ET', 'BitReader', 'ZlibReader', 'ZlibWriter',
    'StringIO', 'read_until', 'read_string', 'pack_string',
    'BitWriter', 'Value', 'writelines', 'pack_utf8',
]

import struct
import array
import zlib
from itertools import chain, imap
try:
    from cStringIO import StringIO
except ImportError:
    from StringIO import StringIO

try:
    import cElementTree as ET
except ImportError:
    try:
        import elementTree.ElementTree as ET
    except ImportError:
        try:
            import lxml.etree as ET
        except ImportError:
            try:
                import xml.etree.ElementTree as ET # python 2.5
            except ImportError:
                ET = None

import dispatch
from dispatch import strategy, functions, combiners, NoApplicableMethods

@dispatch.generic()
def writelines(fileobj, lines):
    pass

@writelines.when("hasattr(fileobj, 'writelines')")
def writelines_writelines(fileobj, lines):
    fileobj.writelines(lines)

@writelines.when(strategy.default)
def writelines_write(fileobj, lines):
    _write = fileobj.write
    for line in lines:
        _write(line)

class Value(object):

    @dispatch.generic()
    def __cmp__(self, other):
        pass

    def __repr__(self):
        return type(self).__name__ + '()'

@Value.__cmp__.when("type(self) is type(other)")
def Value__cmp__default(self, other):
    return cmp(self.__dict__, other.__dict__)

@Value.__cmp__.when(strategy.default)
def Value__cmp__unknown(self, other):
    raise TypeError("No comparison between %s and %s" % (
        type(self).__name__, type(other).__name__))

def _msb_bits(u8):
    for i in xrange(7, -1, -1):
        yield (u8 & (1 << i)) and 1 or 0

def msb_fromstring(s, lst=None):
    if lst is None:
        lst = []
    _extend = lst.extend
    for u8 in array.array('B', s):
        _extend(_msb_bits(u8))
    return lst

def msb_toint(msb):
    res = 0L
    for x in msb:
        res = (res << 1) | (x and 1 or 0)
    return res
   
def iter_only(iterable, cls):
    """
    Iterate over iterable recursively, yielding only values that match
    isinstance(obj, cls)
    """
    iterable = iter((iterable,))
    while True:
        for obj in iterable:
            if not isinstance(obj, cls):
                iterable = chain(obj, iterable)
                break
            yield obj
        else:
            break

def bitflag(name, offset, bits=1):
    if bits < 1:
        raise ValueError("must be at least one bit")
    mask = 0
    for i in xrange(bits):
        mask = (mask << 1) | 1
    mask <<= offset
    def get(self):
        return (getattr(self, name) & mask) >> offset
    def set(self, value):
        value = (value << offset) & mask
        flags = getattr(self, name) & ~mask
        setattr(self, name, flags | value)
    return property(get, set, doc="bitflag%r" % ((name, offset, bits),))
    

def check(expect, f, msg=""):
    got = f.read(len(expect))
    if got != expect:
        if msg:
            msg = msg + ": "
        raise ValueError("%s%r != %r" % (msg, got, expect))

@dispatch.generic()
def read_until(f, terminator):
    pass

@read_until.when(strategy.default)
def read_until_default(f, terminator):
    a = array.array('c')
    _append = a.append
    char = None
    while char != terminator:
        char = f.read(1)
        if char == '':
            raise EOFError("terminator not found")
        _append(char)
    return a.tostring()

@read_until.when("hasattr(f, 'seek') and hasattr(f, 'tell')")
def read_until_seek(f, terminator):
    pos = f.tell()
    buffers = []
    _append = buffers.append
    while True:
        chars = f.read(256)
        if chars == '':
            raise EOFError("terminator not found")
        try:
            idx = chars.index(terminator)
        except ValueError:
            _append(chars)
        else:
            _append(chars[:idx+1])
            break
    res = ''.join(buffers)
    f.seek(pos + len(res))
    return res

class ZlibWriter(object):
    def __init__(self, fileobj, level=9):
        self.fileobj = fileobj
        self.compressobj = zlib.compressobj(level)
        self.position = fileobj.tell()

    def tell(self):
        return self.position

    def close(self):
        self.fileobj.write(self.compressobj.flush())
        self.compressobj = None

    def writelines(self, iterable):
        _compress = self.compressobj.compress
        _write = self.fileobj.write
        for chunk in iterable:
            if not chunk:
                continue
            self.position += len(chunk)
            s = _compress(chunk)
            if s:
                _write(s)

    def write(self, s):
        if not s:
            return
        self.position += len(s)
        s = self.compressobj.compress(s)
        if s:
            self.fileobj.write(s)

class ZlibReader(object):
    def __init__(self, fileobj, bufsize=8192):
        self.fileobj = fileobj
        self.decompressobj = zlib.decompressobj()
        self.rest = array.array('c')
        self.position = fileobj.tell()
        self.eof = False
        self.bufsize = bufsize

    def tell(self):
        return self.position

    def fill(self):
        readbytes = self.fileobj.read(self.bufsize)
        d = self.decompressobj
        if d.unconsumed_tail:
            nextchunk = d.unconsumed_tail + readbytes
        else:
            nextchunk = readbytes
        self.rest.fromstring(d.decompress(nextchunk))
        if len(readbytes) < self.bufsize:
            self.eof = True
            self.rest.fromstring(self.decompressobj.flush())
            self.decompressobj = None

    def read(self, size=-1):
        if size == -1:
            if not self.eof:
                d = self.decompressobj
                chunks.append(d.decompress(self.fileobj.read()))
                chunks.append(d.flush())
                self.decompressobj = None
            rval = self.rest.tostring()
            del self.rest[:]
        else:
            while len(self.rest) < size and not self.eof:
                self.fill()
            if size == 1:
                rval = self.rest.pop(0)
            else:
                rval = self.rest[:size].tostring()
                del self.rest[:size]
        self.position += len(rval)
        return rval

@read_until.when("isinstance(f, ZlibReader)")
def read_until_ZlibReader(f, terminator):
    self = f
    while True:
        try:
            idx = self.rest.index(terminator)
        except ValueError:
            if self.eof:
                self.position += len(self.rest)
                del self.rest[:]
                raise EOFError("terminator not found")
            self.fill()
        else:
            break
    rval = self.read(idx + 1)
    return rval

class BitWriter(object):
    def __init__(self, fileobj=None):
        self.bits = []
        self.fileobj = fileobj

    def flush(self):
        if self.fileobj is None:
            leftover = len(self.bits) & 7
            if leftover:
                self.bits.extend([0] * (8 - leftover))
        else:
            self.fileobj.write(self.tostring())
            del self.bits[:]

    def writebits(self, bits):
        self.bits.extend(bits)

    def writeUB(self, bits, i):
        if not bits:
            return

        _append = self.bits.append
        for place in xrange(bits - 1, -1, -1):
            _append((i & (1 << place)) and 1 or 0)

    def writeSB(self, bits, i):
        if not bits:
            return

        _append = self.bits.append
        bits -= 1
        if i >= 0:
            _append(0)
            return self.writeUB(bits, i)
        _append(1)
        i = -1 - i
        for place in xrange(bits - 1, -1, -1):
            if i & (1 << place):
                _append(0)
            else:
                _append(1)

    def writeFB(self, bits, f):
        self.writeSB(bits, int(f * 65536.0))

    def calcUB(self, args):
        maxnum = max(args)
        if maxnum == 0:
            return 0
        for i in xrange(1, 33):
            maxnum >>= 1
            if maxnum == 0:
                break
        else:
            raise ValueError("out of bounds")
        return i

    def calcSB(self, args):
        return self.calcUB(map(abs, args)) + 1
    
    def calcFB(self, args):
        return self.calcUB([abs(int(i * 65536.0)) for i in args]) + 1

    def tobits(self):
        return self.bits[:]

    def tostring(self):
        chars, rest = divmod(len(self.bits), 8)
        bits = self.bits[:]
        if rest:
            bits.extend([0] * (8 - rest))
            chars += 1
        ints = []
        schars = ['>']
        for fmt, n in ('Q', 64), ('I', 32), ('H', 16), ('B', 8):
            while len(bits) >= n:
                schars.append(fmt)
                ints.append(msb_toint(bits[:n]))
                del bits[:n]
            if len(bits) == 0:
                break
        res = struct.pack(''.join(schars), *ints)
        return res
            

class BitReader(object):
    def __init__(self, fileobj):
        self.fileobj = fileobj
        self.bits = []

    def flush(self):
        del self.bits[:]
   
    def readbits(self, size=None):
        if size is not None and size <= 0:
            raise ValueError("size must be > 0")
        bits = self.bits
        if size is None:
            msb_fromstring(self.fileobj.read(), bits)
            res = bits
            self.bits = []
            return res
        needbits = size - len(bits)
        if needbits > 0:
            rbytes, overflow = divmod(needbits, 8)
            if overflow:
                rbytes += 1
            msb_fromstring(self.fileobj.read(rbytes), bits)
        res = bits[:size]
        del bits[:size]
        return res

    def readUB(self, size):
        return msb_toint(self.readbits(size))

    def readSB(self, size):
        bits = self.readbits(size)
        if bits[0]:
            return -(1 + msb_toint(not x for x in bits[1:]))
        return msb_toint(bits)

    def readFB(self, size):
        return self.readSB(size) / 65536.0

def pack_utf8(s):
    if isinstance(s, unicode):
        s = s.encode('utf-8')
    return struct.pack('>H', len(s)) + s
    
def pack_string(s):
    if isinstance(s, unicode):
        s = s.encode('utf-8')
    return s + '\x00'

def read_string(f):
    # Flash versions prior to 6 used ANSI or Shift-JIS (with no way to tell)
    # Probably not worth caring about that.
    return read_until(f, '\x00')[:-1].decode('utf-8')
