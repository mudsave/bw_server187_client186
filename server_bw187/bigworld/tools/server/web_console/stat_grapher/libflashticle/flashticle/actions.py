import struct

import dispatch
from dispatch import strategy, functions, combiners, NoApplicableMethods

from flashticle.util import StringIO, BitReader, BitWriter, msb_fromstring
from flashticle.util import read_string, pack_string, Value, iter_only

from flashticle.actions_simple import *

def read_actionheader(f, header):
    code = ord(f.read(1))
    if code >= 0x80:
        length = struct.unpack('<H', f.read(2))[0]
    else:
        length = 0
    return code, length

@dispatch.generic()
def read_action(code, length, f, header):
    pass

@dispatch.generic()
def serialize_action(tag, header):
    pass

def pack_actions(actions, header):
    actions = [pack_action(action, header) for action in actions]
    actions.append(chr(ActionEnd.code))
    return ''.join(actions)
        
def pack_action(tag, header):
    iterable = iter(serialize_action(tag, header))
    code = iterable.next()
    if code < 0x80:
        return chr(code)
    rest = ''.join(iter_only(iterable, str))
    return chr(code) + struct.pack('<H', len(rest)) + rest
    
    
class ActionEnd(Value):
    code = 0x00
    def __init__(self):
        raise TypeError("singleton")

@read_action.when("code == ActionEnd.code and length == 0")
def read_action_ActionEnd(code, length, f, header):
    return ActionEnd

class UnknownAction(Value):
    def __init__(self, code, data):
        self.code = code
        self.data = data

    def __repr__(self):
        if self.code < 0x80:
            return '%s(0x%02X)' % (type(self).__name__, self.code)
        return '%s(0x%02X, <%d bytes>)' % (
            type(self).__name__, self.code, len(self.data))

@read_action.when(strategy.default)
def read_action_UnknownAction(code, length, f, header):
    if length:
        data = f.read(length)
    else:
        try:
            cls = SIMPLEACTIONS_CODE[code]
        except KeyError:
            pass
        else:
            return cls()
    return UnknownAction(code, data)

@serialize_action.when("isinstance(tag, UnknownAction)")
def serialize_action_unknown(tag, header):
    return tag.code, tag.data

@serialize_action.when(strategy.default)
def serialize_action_default(tag, header):
    if 0x80 > getattr(tag, 'code', -1) >= 0:
        return (tag.code,)
    raise TypeError("%r can not be serialized" % (tag,))

class ActionConstantPool(Value):
    code = 0x88

    def __init__(self, constantPool):
        self.constantPool = constantPool

    def __repr__(self):
        return '%s(%r)' % (type(self).__name__, self.constantPool)

@read_action.when("code == ActionConstantPool.code")
def read_action_ActionConstantPool(code, length, f, header):
    count = struct.unpack('<H', f.read(2))[0]
    return ActionConstantPool([read_string(f) for i in xrange(count)])

@serialize_action.when("isinstance(tag, ActionConstantPool)")
def serialize_action_ActionConstantPool(tag, header):
    pool = map(pack_string, tag.constantPool)
    return tag.code, struct.pack('<H', len(pool)), ''.join(pool)

# XXX: unused
ACTIONPUSH_TYPES = {
    0: 'string literal',
    1: 'floating-point literal',
    2: 'null',
    3: 'undefined',
    4: 'register',
    5: 'boolean',
    6: 'double',
    7: 'integer',
    8: 'constant 8',
    9: 'constant 16',
}

# XXX: unused
class Register(Value):
    def __init__(self, number):
        self.number = number

    def __repr__(self):
        return '%s(%r)' % (type(self).__name__, self.number)

class ConstantPoolIndex(Value):
    def __init__(self, index):
        self.index = index

    def __repr__(self):
        return '%s(%r)' % (type(self).__name__, self.index)

class ActionPush(Value):
    code = 0x96
    def __init__(self, *values):
        self.values = list(values)

    def __repr__(self):
        return '%s(%s)' % (type(self).__name__, ', '.join(map(repr, self.values)))

class Undefined(Value):
    def __repr__(self):
        return 'undefined'

undefined = Undefined()

class UnknownActionPushValue(Value):
    def __init__(self, code, data):
        self.code = code
        self.data = data

    def __repr__(self):
        return '%s(0x%02X, %r)' % (type(self).__name__, self.code, self.data)

@dispatch.generic()
def actionpush_packvalue(value, header):
    pass

@actionpush_packvalue.when("isinstance(value, basestring)")
def actionpush_packvalue_basestring(value, header):
    return '\x00' + pack_string(value)
    
# We don't pack single precision '\x01' + struct.pack('<f', ...)

@actionpush_packvalue.when("value is None")
def actionpush_packvalue_None(value, header):
    return '\x02'

@actionpush_packvalue.when("value is undefined")
def actionpush_packvalue_undefined(value, header):
    return '\x03'

@actionpush_packvalue.when("isinstance(value, Register)")
def actionpush_packvalue_Register(value, header):
    return '\x05' + chr(value.number)

@actionpush_packvalue.when("isinstance(value, bool)")
def actionpush_packvalue_bool(value, header):
    return '\x05' + (value and '\x01' or '\x00')

@actionpush_packvalue.when("isinstance(value, float)")
def actionpush_packvalue_float(value, header):
    return '\x06' + struct.pack('<d', value)

@actionpush_packvalue.when("isinstance(value, (int, long))")
def actionpush_packvalue_int(value, header):
    return '\x07' + struct.pack('<i', value)

@actionpush_packvalue.when("isinstance(value, ConstantPoolIndex)")
def actionpush_packvalue_ConstantPoolIndex(value, header):
    if value.index <= 0xffff:
        return '\x08' + chr(value.index)
    return '\x09' + struct.pack('<H', value.index)

@actionpush_packvalue.when("isinstance(value, UnknownActionPushValue)")
def actionpush_packvalue_UnknownActionPushValue(value, header):
    return chr(value.code) + value.data

@serialize_action.when("isinstance(tag, ActionPush)")
def serialize_action_ActionPush(tag, header):
    return tag.code, [actionpush_packvalue(v, header) for v in tag.values]

@read_action.when("code == ActionPush.code")
def read_action_ActionPush(code, length, f, header):
    values = []
    data = f.read(length)
    while data:
        typeCode, data = ord(data[0]), data[1:]
        if typeCode == 0:
            idx = data.index('\x00')
            value = data[:idx].decode('utf8')
            data = data[idx + 1:]
        elif typeCode == 1:
            value = struct.unpack('<f', data[:4])[0]
            data = data[4:]
        elif typeCode == 2:
            value = None
        elif typeCode == 3:
            value = undefined
        elif typeCode == 4:
            value = Register(ord(data[0]))
            data = data[1:]
        elif typeCode == 5:
            value = (data[0] != '\x00')
            data = data[1:]
        elif typeCode == 6:
            value = struct.unpack('<d', data[:8])[0]
            data = data[8:]
        elif typeCode == 7:
            value = struct.unpack('<i', data[:4])[0]
            data = data[4:]
        elif typeCode == 8:
            value = ConstantPoolIndex(ord(data[0]))
            data = data[1:]
        elif typeCode == 9:
            value = ConstantPoolIndex(struct.unpack('<H', data[:4])[0])
            data = data[4:]
        else:
            value = UnknownActionPushValue(typeCode, data)
            data = ''
        values.append(value)
    return ActionPush(*values)
    
class ActionBranch(Value):
    def __init__(self, branchOffset):
        self.branchOffset = branchOffset

    def __repr__(self):
        return '%s(%r)' % (type(self).__name__, self.branchOffset)

class ActionIf(ActionBranch):
    code = 0x9d

class ActionJump(ActionBranch):
    code = 0x99

def read_action_ActionBranch(cls, code, length, f, header):
    return cls(struct.unpack('<h', f.read(2))[0])

@read_action.when("code == ActionIf.code")
def read_action_ActionIf(code, length, f, header):
    return read_action_ActionBranch(ActionIf, code, length, f, header)

@read_action.when("code == ActionJump.code")
def read_action_ActionJump(code, length, f, header):
    return read_action_ActionBranch(ActionJump, code, length, f, header)

@serialize_action.when("isinstance(tag, ActionBranch)")
def serialize_action_ActionBranch(tag, header):
    return tag.code, struct.pack('<h', tag.branchOffset)

class ActionGetURL(Value):
    code = 0x83
    def __init__(self, urlString, targetString):
        self.urlString = urlString
        self.targetString = targetString

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (
            self.urlString, self.targetString))

@read_action.when("code == ActionGetURL.code")
def read_action_ActionGetURL(code, length, f, header):
    urlString = read_string(f)
    targetString = read_string(f)
    return ActionGetURL(urlString, targetString)

@serialize_action.when("isinstance(tag, ActionGetURL)")
def serialize_action_ActionGetURL(tag, header):
    return tag.code, pack_string(tag.urlString) + pack_string(tag.targetString)

class ActionGetURL2(Value):
    code = 0x9A
    def __init__(self, method, targetFlag, variablesFlag):
        self.method = method
        self.targetFlag = targetFlag
        self.variablesFlag = variablesFlag

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (
            self.method, self.targetFlag, self.variablesFlag))

@read_action.when("code == ActionGetURL2.code")
def read_action_ActionGetURL2(code, length, f, header):
    br = BitReader(f)
    method = br.readUB(2)
    ignore = br.readUB(4)
    targetFlag, variablesFlag = br.readbits(2)
    return ActionGetURL2(method, targetFlag, variablesFlag)

@serialize_action.when("isinstance(tag, ActionGetURL2)")
def serialize_action_ActionGetURL2(tag, header):
    br = BitWriter()
    br.writeUB(2, tag.method)
    br.writeUB(4, 0)
    br.writebits([tag.targetFlag, tag.variablesFlag])
    return tag.code, br.tostring()

class ActionDefineFunction2(Value):
    code = 0x8e
    def __init__(self, name, registerCount, preloadParent, preloadRoot,
            suppressSuper, preloadSuper, suppressArguments, preloadArguments,
            suppressThis, preloadThis, preloadGlobal, parameters, actions):
        self.name = name
        self.registerCount = registerCount
        self.preloadParent = preloadParent
        self.preloadRoot = preloadRoot
        self.suppressSuper = suppressSuper
        self.preloadSuper = preloadSuper
        self.suppressArguments = suppressArguments
        self.preloadArguments = preloadArguments
        self.suppressThis = suppressThis
        self.preloadThis = preloadThis
        self.preloadGlobal = preloadGlobal
        self.parameters = parameters
        self.code = code
        self.actions = actions

class RegisterParam(Value):
    def __init__(self, register, name):
        self.register = register
        self.name = name

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (self.register, self.name))

# XXX: Disabled
#@read_action.when("code == ActionDefineFunction2.code")
def read_action_ActionDefineFunction2(code, length, f, header):
    name = read_string(f)
    numParams, registerCount = struct.unpack('<HB', f.read(3))
    br = BitReader(f)
    bits = br.readbits(16)
    preloadParent, preloadRoot, suppressSuper, preloadSuper = bits[:4]
    suppressArguments, preloadArguments, suppressThis, preloadThis = bits[4:8]
    preloadGlobal = bits[15]
    parameters = [
        RegisterParam(ord(f.read(1)), read_string(f))
        for i in xrange(numParams)
    ]
    codeSize = struct.unpack('<H', f.read(2))[0]
    # This doesn't seem to be how to read the actions for a function
    actions = list(read_actions(f, header))
    return ActionDefineFunction2(name, registerCount, preloadParent,
        preloadRoot, suppressSuper, preloadSuper, suppressArguments,
        preloadArguments, suppressThis, preloadThis, preloadGlobal,
        parameters, actions)


def read_actions(f, header):
    while True:
        code, length = read_actionheader(f, header)
        action = read_action(code, length, f, header)
        if action is ActionEnd:
            break
        yield action

class ClipEventFlags(Value):
    def __init__(self, keyUp=None, keyDown=None, mouseUp=None,
            mouseDown=None, mouseMove=None, unload=None, enterFrame=None,
            load=None, dragOver=None, rollOut=None, rollOver=None,
            releaseOutside=None, release=None, press=None, initialize=None,
            data=None, construct=None, keyPress=None, dragOut=None):
        self.keyUp = keyUp
        self.keyDown = keyDown
        self.mouseUp = mouseUp,
        self.mouseDown = mouseDown
        self.mouseMove = mouseMove
        self.unload = unload
        self.enterFrame = enterFrame,
        self.load = load
        self.dragOver = dragOver
        self.rollOut = rollOut
        self.rollOver = rollOver,
        self.releaseOutside = releaseOutside
        self.release = release
        self.press = press
        self.initialize = initialize,
        self.data = data
        self.construct = construct
        self.keyPress = keyPress
        self.dragOut = dragOut

def unpack_ClipEventFlags(s, header):
    newstyle = header.sig.version >= 6
    bits = msb_fromstring(s)
    keyUp, keyDown, mouseUp, mouseDown, \
        mouseMove, unload, enterFrame, \
        load, dragOver, rollOut, rollOver, \
        releaseOutside, release, press, \
        initialize, data = bits[:16]
    if newstyle:
        #ignore = br.readbits(5)
        construct, keyPress, dragOut = bits[16 + 5:16 + 8]
        #ignore = f.read(1)
    else:
        construct, keyPress, dragOut = None, None, None
    return ClipEventFlags(keyUp, keyDown, mouseUp, mouseDown,
        mouseMove, unload, enterFrame, load, dragOver, rollOut, rollOver,
        releaseOutside, release, press, initialize, data,
        construct, keyPress, dragOut)

def pack_ClipEventFlags(self, header):
    bits = [
        self.keyUp, self.keyDown, self.mouseUp, self.mouseDown,
        self.mouseMove, self.unload, self.enterFrame, self.load,
        self.dragOver, self.rollOut, self.rollOver, self.releaseOutside,
        self.release, self.press, self.initialize, self.data
    ]
    if header.sig.version >= 6:
        bits.extend([0] * 5)
        bits.extend([self.construct, self.keyPress, self.dragOut])
        bits.extend([0] * 8)
        fmt = '>I'
    else:
        fmt = '>H'
    return struct.pack(fmt, msb_toint(bits))
    
def read_ClipEventFlags(f, header):
    newstyle = header.sig.version >= 6
    if newstyle:
        return unpack_ClipEventFlags(f.read(4), header)
    else:
        return unpack_ClipEventFlags(f.read(2), header)

   
class ClipActionRecord(Value):
    def __init__(self, flags, data):
        self.flags = flags
        # XXX: contains actions
        self.data = data

def pack_ClipActionRecord(self, header):
    return (
        pack_ClipEventFlags(self.flags, header) +
        struct.pack('<I', len(self.data)) +
        self.data
    )

def serialize_clip_actions(self, header):
    yield '\x00\x00'
    yield pack_ClipEventFlags(self.flags, header)
    for rec in self.records:
        yield pack_ClipActionRecord(rec, header)
    yield '\x00' * ((header.sig.version >= 6) and 4 or 2)

def read_all_ClipActionRecord(f, header):
    count = (header.sig.version >= 6) and 4 or 2
    endflag = '\x00' * count
    while True:
        flags = f.read(count)
        if flags == endflag:
            break
        flags = unpack_ClipEventFlags(flags, header)
        size = struct.unpack('<I', f.read(4))[0]
        yield ClipActionRecord(flags, f.read(size))
    
def read_clip_actions(f, header):
    reserved = f.read(2)
    allEventFlags = read_ClipEventFlags(f, header)
    records = list(read_all_ClipActionRecord(f, header))
