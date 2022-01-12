import struct
import shutil

import dispatch
from dispatch import strategy, functions, combiners, NoApplicableMethods

from flashticle.actions import read_actions, pack_actions
from flashticle.actions import read_clip_actions, serialize_clip_actions
from flashticle.util import StringIO, BitReader, ZlibReader, read_string
from flashticle.util import bitflag, Value, pack_string, iter_only
from flashticle.util import BitWriter, ZlibWriter, writelines, msb_toint

LANGUAGECODES = {
    1: 'Latin',
    2: 'Japanese',
    3: 'Korean',
    4: 'Simplified Chinese',
    5: 'Traditional Chinese',
}

TWIP = 0.05

class Signature(Value):
    def __init__(self, compressed, version, fileLength):
        self.compressed = compressed
        self.version = version
        self.fileLength = fileLength

    def __repr__(self):
        return '%s%r' % (type(self).__name__,
            (self.compressed, self.version, self.fileLength))

class Header(Value):
    def __init__(self, signature, frameSize, frameRate, frameCount):
        self.signature = signature
        self.frameSize = frameSize
        self.frameRate = frameRate
        self.frameCount = frameCount

    def __repr__(self):
        return '%s%r' % (type(self).__name__,
            (self.signature, self.frameSize, self.frameRate, self.frameCount))
            
        
class RGB(Value):
    def __init__(self, red, green, blue):
        self.red = red
        self.green = green
        self.blue = blue

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (self.red, self.green, self.blue))

def read_rgb(f):
    return RGB(*map(ord, f.read(3)))

def pack_rgb(self):
    return struct.pack('<BBB', self.red, self.green, self.blue)

def write_rgb(self, f):
    f.write(pack_rgb(self))

class RGBA(Value):
    def __init__(self, red, green, blue, alpha):
        self.red = red
        self.green = green
        self.blue = blue
        self.alpha = alpha

    def __repr__(self):
        return '%s%r' % (
            type(self).__name__, self.red, self.green, self.blue, self.alpha)

def read_rgba(f):
    return RGBA(*map(ord, f.read(4)))

def pack_rgba(self):
    return struct.pack('<BBBB', self.red, self.green, self.blue, self.alpha)

def write_rgba(self, f):
    f.write(pack_rgba(self))

class Rect(Value):
    def __init__(self, xmin, xmax, ymin, ymax):
        self.xmin = xmin
        self.xmax = xmax
        self.ymin = ymin
        self.ymax = ymax

    width = property(lambda s: s.xmax - s.xmin)
    height = property(lambda s: s.ymax - s.ymin)

    def __repr__(self):
        return '%s%r' % (
            type(self).__name__, (self.xmin, self.xmax, self.ymin, self.ymax))

def read_rect(br):
    nbits = br.readUB(5)
    return Rect(*(br.readSB(nbits) * TWIP for i in xrange(4)))

def write_rect(self, bw):
    args = [int(i / TWIP) for i in (self.xmin, self.xmax, self.ymin, self.ymax)]
    bits = bw.calcSB(args)
    bw.writeSB(5, bits)
    for i in args:
        bw.writeSB(bits, i)

    
class Matrix(Value):
    def __init__(self, scale, rotate, translate):
        self.scale = scale
        self.rotate = rotate
        self.translate = translate

    def __repr__(self):
        return '%s%r' % (
            type(self).__name__, (self.scale, self.rotate, self.translate))

def read_gradient(br):
    raise NotImplementedError

def write_gradient(self, br):
    raise NotImplementedError

def read_matrix(br):
    br.flush()

    # hasScale
    if br.readbits(1)[0]:
        n = br.readUB(5)
        scale = br.readFB(n), br.readFB(n)
    else:
        scale = None
    
    # hasRotate
    if br.readbits(1)[0]:
        n = br.readUB(5)
        rotate = br.readFB(n), br.readFB(n)
    else:
        rotate = None

    # translate
    n = br.readUB(5)
    if n:
        translate = br.readSB(n) * TWIP, br.readSB(n) * TWIP
    else:
        translate = 0.0, 0.0
    return Matrix(scale, rotate, translate)

def write_matrix(self, bw):
    bw.flush()
    if self.scale:
        bw.writebits([1])
        bits = bw.calcFB(self.scale)
        bw.writeUB(5, bits)
        for i in self.scale:
            bw.writeFB(bits, i)
    else:
        bw.writebits([0])
    if self.rotate:
        bw.writebits([1])
        bits = bw.calcFB(self.rotate)
        bw.writeUB(5, bits)
        for i in self.rotate:
            bw.writeFB(bits, i)
    else:
        bw.writebits([0])
    args = [int(i / TWIP) for i in self.translate]
    bits = bw.calcSB(args)
    bw.writeUB(5, bits)
    for i in args:
        bw.writeSB(bits, i)
    
class ColorTransform(Value):
    def __init__(self, add, mul):
        self.add = add
        self.mul = mul

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (self.add, self.mul))

def read_color_transform(br):
    br.flush()
    hasAdd, hasMul = br.readbits(2)
    n = br.readUB(4)
    if hasMul:
        mul = br.readSB(n), br.readSB(n), br.readSB(n)
    else:
        mul = None
    if hasAdd:
        add = br.readSB(n), br.readSB(n), br.readSB(n)
    else:
        add = None
    return ColorTransform(add, mul)

def write_color_transform(self, bw):
    bw.flush()
    bw.writebits([self.add and 1 or 0, self.mul and 1 or 0])
    args = []
    if self.mul:
        args.extend(self.mul)
    if self.add:
        args.extend(self.add)
    if not args:
        bw.writeUB(4, 0)
    else:
        bits = bw.calcSB(args)
        bw.writeUB(4, bits)
        for i in args:
            bw.writeSB(bits, i)

class ColorTransformWithAlpha(Value):
    def __init__(self, add, mul):
        self.add = add
        self.mul = mul

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (self.add, self.mul))

def read_color_transform_with_alpha(br):
    br.flush()
    hasAdd, hasMul = br.readbits(2)
    n = br.readUB(4)
    if hasMul:
        mul = br.readSB(n), br.readSB(n), br.readSB(n), br.readSB(n)
    else:
        mul = None
    if hasAdd:
        add = br.readSB(n), br.readSB(n), br.readSB(n), br.readSB(n)
    else:
        add = None
    return ColorTransform(add, mul)

def write_color_transform_with_alpha(self, bw):
    write_color_transform(self, bw)

class FillStyle(Value):
    fillStyleType = None

class FillStyleSolid(FillStyle):
    fillStyleType = 0x00
    def __init__(self, color):
        self.color = color

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (self.color,))

class FillStyleGradient(FillStyle):
    def __init__(self, gradientMatrix, gradient):
        self.gradientMatrix = gradientMatrix
        self.gradient = gradient

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (self.gradientMatrix, self.gradient))

class FillStyleGradientLinear(FillStyleGradient):
    fillStyleType = 0x10

class FillStyleGradientRadial(FillStyleGradient):
    fillStyleType = 0x12

class FillStyleBitmap(FillStyle):
    def __init__(self, bitmapID, bitmapMatrix):
        self.bitmapID = bitmapID
        self.bitmapMatrix = bitmapMatrix

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (self.bitmapID, self.bitmapMatrix))

class FillStyleBitmapTiled(FillStyleBitmap):
    fillStyleType = 0x40

class FillStyleBitmapClipped(FillStyleBitmap):
    fillStyleType = 0x41

class LineStyle(Value):
    def __init__(self, width, color):
        self.width = width
        self.color = color

def read_fill_style(f, ctx):
    style = ord(f.read(1))
    if style == 0:
        if ctx.version >= 3:
            color = read_rgba(f)
        else:
            color = read_rgb(f)
        return FillStyleSolid(color)
    elif style == 0x10 or style == 0x12:
        br = BitReader(f)
        gradientMatrix = read_matrix(br)
        gradient = read_gradient(br)
        if style == 0x10:
            cls = FillStyleGradientLinear
        else:
            cls = FillStyleGradientRadial
        return cls(gradientMatrix, gradient)
    elif style == 0x40 or style == 0x41:
        bitmapID = struct.unpack('<H', f.read(2))[0]
        bitmapMatrix = read_matrix(BitReader(f))
        if style == 0x40:
            cls = FillStyleBitmapTiled
        else:
            cls = FillStyleBitmapClipped
        return cls(bitmapID, bitmapMatrix)
    raise ValueError("Unknown fill style: 0x%02X" % (style,))

def pack_fill_style(style, ctx):
    typ = style.fillStyleType
    if typ == 0:
        if ctx.version >= 3:
            cpack = pack_rgba
        else:
            cpack = pack_rgb
        return chr(typ) + cpack(style.color)
    elif typ == 0x10 or typ == 0x12:
        bw = BitWriter()
        write_matrix(style.gradientMatrix, bw)
        write_gradient(style.gradient, bw)
        return chr(typ) + bw.tostring()
    elif typ == 0x40 or typ == 0x41:
        bw = BitWriter()
        write_matrix(style.bitmapMatrix, bw)
        return chr(typ) + struct.pack('<H', style.bitmapID) + bw.tostring()
    raise ValueError("Unknown fill style: 0x%02X" % (style,))

def read_line_style(f, ctx):
    width = struct.unpack('<H', f.read(2))[0]
    if ctx.version >= 3:
        color = read_rgba(f)
    else:
        color = read_rgb(f)
    return LineStyle(width, color)

def pack_line_style(style, ctx):
    width = struct.pack('<H', style.width)
    if ctx.version >= 3:
        cpack = pack_rgba
    else:
        cpack = pack_rgb
    return width + cpack(style.color)
    
def read_count_ex(f, read_one, ctx=None):
    count = ord(f.read(1))
    if count == 0xFF:
        count = struct.unpack('<H', f.read(2))[0]
    return [read_one(f, ctx) for i in xrange(count)]

def pack_count_ex(lst, pack_one, ctx=None):
    num = len(lst)
    if num < 0xFF:
        yield chr(num)
    else:
        yield struct.pack('<BH', 0xFF, num)
    for item in lst:
        yield pack_one(item, ctx)

class StyleChangeRecord(Value):
    def __init__(self, move=None, fillStyle0=None, fillStyle1=None,
            lineStyle=None, fillStyles=None, lineStyles=None):
        self.move = move
        self.fillStyle0 = fillStyle0
        self.fillStyle1 = fillStyle1
        self.lineStyle = lineStyle
        self.fillStyles = fillStyles
        self.lineStyles = lineStyles

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (
                self.move,
                self.fillStyle0, self.fillStyle1,
                self.lineStyle,
                self.fillStyles, self.lineStyles))

def read_style_change_record(br, flags, readbits):
    move = None
    fillStyle0 = None
    fillStyle1 = None
    lineStyle = None
    fillStyles = None
    lineStyles = None

    if flags[4]:
        # moveTo
        moveBits = br.readUB(5)
        move = br.readSB(moveBits) * TWIP, br.readSB(moveBits) * TWIP
    if flags[3]:
        # fillStyle0
        fillStyle0 = br.readUB(readbits[0])
    if flags[2]:
        # fillStyle1
        fillStyle1 = br.readUB(readbits[0])
    if flags[1]:
        # lineStyle
        lineStyle = br.readUB(readbits[1])
    if flags[0]:
        # newStyles
        raise NotImplementedError
        f = br.fileobj
        fillStyles = read_count_ex(f, read_fill_style, ctx)
        lineStyles = read_count_ex(f, read_line_style, ctx)
        br.flush()
        readbits[0] = br.readUB(4)
        readbits[1] = br.readUB(4)
    
    return StyleChangeRecord(move=move,
        fillStyle0=fillStyle0, fillStyle1=fillStyle1,
        lineStyle=lineStyle,
        fillStyles=fillStyles, lineStyles=lineStyles)

def write_style_change_record(s, bw, writebits):
    flags = [0, 0, 0, 0, 0]
    subw = BitWriter()
    if s.move is not None:
        flags[4] = 1
        mx, my = s.move
        nums = map(int, [mx / TWIP, my / TWIP])
        moveBits = subw.calcSB(nums)
        subw.writeUB(5, moveBits)
        for num in nums:
            subw.writeSB(moveBits, num)
    if s.fillStyle0 is not None:
        flags[3] = 1
        subw.writeUB(writebits[0], s.fillStyle0)
    if s.fillStyle1 is not None:
        flags[2] = 1
        subw.writeUB(writebits[0], s.fillStyle1)
    if s.lineStyle is not None:
        flags[1] = 1
        subw.writeUB(writebits[1], s.lineStyle)
    if s.fillStyles is not None or s.lineStyles is not None:
        # XXX: newStyles
        flags[0] = 1
        raise NotImplementedError
    bw.writebits(flags)
    bw.writebits(subw.tobits())

class StraightEdgeRecord(Value):
    def __init__(self, delta):
        self.delta = delta

    def __repr__(self):
        return '%s(%r)' % (type(self).__name__, self.delta)

def read_straight_edge_record(br):
    numBits = br.readUB(4) + 2
    lineFlag = br.readUB(1)
    if lineFlag:
        delta = br.readSB(numBits) * TWIP, br.readSB(numBits) * TWIP
    else:
        vertFlag = br.readUB(1)
        if vertFlag:
            delta = 0, br.readSB(numBits) * TWIP
        else:
            delta = br.readSB(numBits) * TWIP, 0
    return StraightEdgeRecord(delta)

def write_straight_edge_record(shape, bw):
    dx, dy = [int(x / TWIP) for x in shape.delta]
    if dx and dy:
        nums = [dx, dy]
        flags = [1]
    else:
        if dx == 0:
            flags = [0, 1]
            nums = [dy]
        else:
            flags = [0, 0]
            nums = [dx]
    numBits = max(2, bw.calcSB(nums))
    bw.writeUB(4, numBits - 2)
    bw.writebits(flags)
    for num in nums:
        bw.writeSB(numBits, num)


class CurvedEdgeRecord(Value):
    def __init__(self, control, anchor):
        self.control = control
        self.anchor = anchor

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (self.control, self.anchor))

def read_curved_edge_record(br):
    numBits = br.readUB(4) + 2
    control = br.readSB(numBits) * TWIP, br.readSB(numBits) * TWIP
    anchor = br.readSB(numBits) * TWIP, br.readSB(numBits) * TWIP
    return CurvedEdgeRecord(control, anchor)

def read_shape_record(br, readbits):
    typeRecord = br.readUB(1)
    if typeRecord == 0:
        # Not an edge record
        flags = br.readbits(5)
        # EndShapeRecord
        if flags == [0, 0, 0, 0, 0]:
            return None
        return read_style_change_record(br, flags, readbits)
    straightFlag = br.readUB(1)
    if straightFlag:
        return read_straight_edge_record(br)
    return read_curved_edge_record(br)

def write_shape_record(shape, bw, writebits):
    if isinstance(shape, StyleChangeRecord):
        bw.writebits([0])
        write_style_change_record(shape, bw, writebits)
    else:
        if isinstance(shape, StraightEdgeRecord):
            bw.writebits([1, 1])
            write_straight_edge_record(shape, bw)
        elif isinstance(shape, CurvedEdgeRecord):
            bw.writebits([1, 0])
            write_curved_edge_record(shape, bw)
        else:
            raise TypeError("unexpected edge record: %r" % (shape,))

def write_curved_edge_record(shape, bw):
    c = shape.control
    a = shape.anchor
    nums = map(int, [c[0] / TWIP, c[1] / TWIP, a[0] / TWIP, a[1] / TWIP])
    numBits = max(2, bw.calcSB(nums))
    bw.writeUB(4, numBits - 2)
    for num in nums:
        bw.writeSB(numBits, num)

def traverse_shape_record(s):
    if isinstance(s, StyleChangeRecord):
        return [s.fillStyle0, s.fillStyle1], [s.lineStyle]
    return [], []
    
class ShapeWithStyle(Value):
    def __init__(self, fillStyles, lineStyles, shapes):
        self.fillStyles = fillStyles
        self.lineStyles = lineStyles
        self.shapes = shapes

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (
            self.fillStyles, self.lineStyles, self.shapes))

def read_shape_with_style(f, ctx):
    fillStyles = read_count_ex(f, read_fill_style, ctx)
    lineStyles = read_count_ex(f, read_line_style, ctx)

    br = BitReader(f)
    fillBits = br.readUB(4)
    lineBits = br.readUB(4)
    readbits = [fillBits, lineBits]
    shapes = []
    while True:
        shape = read_shape_record(br, readbits)
        if shape is None:
            break
        shapes.append(shape)
    return ShapeWithStyle(fillStyles, lineStyles, shapes)

def pack_shape_with_style(shape, ctx):
    yield pack_count_ex(shape.fillStyles, pack_fill_style, ctx)
    yield pack_count_ex(shape.lineStyles, pack_line_style, ctx)

    lineints = []
    fillints = []
    for s in shape.shapes:
        l, f = traverse_shape_record(s)
        lineints.extend(l)
        fillints.extend(f)
    lineints = filter(None, lineints) or [0]
    fillints = filter(None, fillints) or [0]

    bw = BitWriter()
    writebits = [bw.calcUB(nums) for nums in lineints, fillints]
    
    for bits in writebits:
        bw.writeUB(4, bits)

    for s in shape.shapes:
        write_shape_record(s, bw, writebits)
    
    bw.writebits([0, 0, 0, 0, 0, 0])

    yield bw.tostring()


def read_signature(f):
    signature, version, fileLength = struct.unpack('<3sBI', f.read(8))
    if signature == 'FWS':
        compressed = False
    elif signature == 'CWS':
        compressed = True
    else:
        raise ValueError("%r is not a known SWF signature" % (signature,))
    if compressed and version < 6:
        raise ValueError("SWF version %d can not be compressed" % (version,))
    return Signature(compressed, version, fileLength)

def read_header(f, signature):
    frameSize = read_rect(BitReader(f))
    frameRate, frameCount = struct.unpack('<HH', f.read(4))
    frameRate /= 256.0
    return Header(signature, frameSize, frameRate, frameCount)

def write_header(header, f):
    bw = BitWriter(f)
    write_rect(header.frameSize, bw)
    bw.flush()
    f.write(struct.pack('<HH', int(header.frameRate * 256), header.frameCount))

def from_swf(f):
    signature = read_signature(f)
    if signature.compressed:
        f = ZlibReader(f)
    header = read_header(f, signature)
    return header, read_body(f, header)

def read_tagheader(f):
    codeAndLength = struct.unpack('<H', f.read(2))[0]
    code = codeAndLength >> 6
    length = codeAndLength & 0x3f
    if length == 0x3f:
        length = struct.unpack('<I', f.read(4))[0]
    return code, length

@dispatch.generic()
def serialize_tag(tag, header):
    pass

@dispatch.generic()
def read_tag(code, length, f, header):
    pass

class SetBackgroundColor(Value):
    code = 9
    def __init__(self, rgb):
        self.rgb = rgb

    def __repr__(self):
        return '%s(%r)' % (type(self).__name__, self.rgb)

@read_tag.when("code == SetBackgroundColor.code")
def read_tag_SetBackgroundColor(code, length, f, header):
    return SetBackgroundColor(read_rgb(f))

@serialize_tag.when("isinstance(tag, SetBackgroundColor)")
def serialize_tag_SetBackgroundColor(tag, header):
    return tag.code, pack_rgb(tag.rgb)

class DefineBitsLossless2(Value):
    code = 36
    def __init__(self, characterID, format, width, height, data):
        self.characterID = characterID
        self.format = format
        self.width = width
        self.height = height
        self.data = data

    def __repr__(self):
        return '%s(%r, %r, %r, %r, <%d bytes>)' % (
            type(self).__name__, self.characterID, self.format,
            self.width, self.height, len(self.data))


@read_tag.when("code == DefineBitsLossless2.code")
def read_tag_DefineBitsLossless2(code, length, f, header):
    characterID, fmt, w, h = struct.unpack('<HBHH', f.read(7))
    data = f.read(length - 7)
    return DefineBitsLossless2(characterID, fmt, w, h, data)

@serialize_tag.when("isinstance(tag, DefineBitsLossless2)")
def serialize_tag_DefineBitsLossless2(tag, header):
    d = struct.pack('<HBHH', tag.characterID, tag.format, tag.width, tag.height)
    return tag.code, d, tag.data


class DefineSprite(Value):
    code = 39
    def __init__(self, spriteID, frameCount, controlTags):
        self.spriteID = spriteID
        self.frameCount = frameCount
        self.controlTags = controlTags

    def __repr__(self):
        return '%s%r' % (type(self).__name__,
            (self.spriteID, self.frameCount, self.controlTags))

@read_tag.when("code == DefineSprite.code")
def read_tag_DefineSprite(code, length, f, header):
    spriteID, frameCount = struct.unpack('<HH', f.read(4))
    return DefineSprite(spriteID, frameCount, list(read_body(f, header)))

@serialize_tag.when("isinstance(tag, DefineSprite)")
def serialize_tag_DefineSprite(tag, header):
    return (
        tag.code,
        struct.pack('<HH', tag.spriteID, tag.frameCount),
        pack_tags(tag.controlTags, header),
    )

class ExportAssets(Value):
    code = 56
    def __init__(self, exports):
        self.exports = exports

    def __repr__(self):
        return '%s(%r)' % (type(self).__name__, self.exports)

@read_tag.when("code == ExportAssets.code")
def read_tag_ExportAssets(code, length, f, header):
    count = struct.unpack('<H', f.read(2))[0]
    exports = [
        (struct.unpack('<H', f.read(2))[0], read_string(f))
        for i in xrange(count)
    ]
    return ExportAssets(exports)

@serialize_tag.when("isinstance(tag, ExportAssets)")
def serialize_tag_ExportAssets(tag, header):
    exports = [(struct.pack('<H', a) + pack_string(b)) for a, b in tag.exports]
    return tag.code, struct.pack('<H', len(exports)), ''.join(exports)

class DoInitAction(Value):
    code = 59
    def __init__(self, spriteID, actions):
        self.spriteID = spriteID
        self.actions = actions

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (self.spriteID, self.actions))
   
@read_tag.when("code == DoInitAction.code")
def read_tag_DoInitAction(code, length, f, header):
    spriteID = struct.unpack('<H', f.read(2))[0]
    actions = list(read_actions(f, header))
    return DoInitAction(spriteID, actions)

@serialize_tag.when("isinstance(tag, DoInitAction)")
def serialize_tag_DoInitAction(tag, header):
    return (
        tag.code,
        struct.pack('<H', tag.spriteID),
        pack_actions(tag.actions, header),
    )

class DoAction(Value):
    code = 12
    def __init__(self, actions):
        self.actions = actions

    def __repr__(self):
        return '%s(%r)' % (type(self).__name__, self.actions)

@read_tag.when("code == DoAction.code")
def read_tag_DoAction(code, length, f, header):
    return DoAction(list(read_actions(f, header)))

@serialize_tag.when("isinstance(tag, DoAction)")
def serialize_tag_DoAction(tag, header):
    return tag.code, pack_actions(tag.actions, header)


class ShowFrame(Value):
    code = 1
    def __repr__(self):
        return type(self).__name__ + '()'

@read_tag.when("code == ShowFrame.code and length == 0")
def read_tag_ShowFrame(code, length, f, header):
    return ShowFrame()

@serialize_tag.when("isinstance(tag, ShowFrame)")
def serialize_tag_ShowFrame(tag, header):
    return (tag.code,)

class DefineShape(Value):
    code = 2
    version = 1
    def __init__(self, shapeID, bounds, shapes):
        self.shapeID = shapeID
        self.bounds = bounds
        self.shapes = shapes

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (
            self.shapeID, self.bounds, self.shapes))

class DefineShape2(DefineShape):
    code = 22
    version = 2

class DefineShape3(DefineShape):
    code = 32
    version = 3

def read_tag_DefineShape_ex(cls, code, length, f, header):
    shapeID = struct.unpack('<H', f.read(2))[0]
    bits = BitReader(f)
    bounds = read_rect(bits)
    shapes = read_shape_with_style(f, cls)
    return cls(shapeID, bounds, shapes)

@read_tag.when("code == DefineShape.code")
def read_tag_DefineShape(code, length, f, header):
    return read_tag_DefineShape_ex(DefineShape, code, length, f, header)

@read_tag.when("code == DefineShape2.code")
def read_tag_DefineShape(code, length, f, header):
    return read_tag_DefineShape_ex(DefineShape2, code, length, f, header)

@read_tag.when("code == DefineShape3.code")
def read_tag_DefineShape(code, length, f, header):
    return read_tag_DefineShape_ex(DefineShape3, code, length, f, header)

@serialize_tag.when("isinstance(tag, DefineShape)")
def serialize_tag_DefineShape(tag, header):
    shapeID = struct.pack('<H', tag.shapeID)
    bw = BitWriter()
    write_rect(tag.bounds, bw)
    shapes = pack_shape_with_style(tag.shapes, type(tag))
    return tag.code, shapeID, bw.tostring(), shapes

class DefineSound(Value):
    code = 14
    def __init__(self, soundID, flags, sampleCount, data):
        self.soundID = soundID
        self.flags = flags
        self.sampleCount = sampleCount
        self.data = data

    format = bitflag("flags", 4, 4)
    rate = bitflag("flags", 2, 2)
    size = bitflag("flags", 1)
    soundType = bitflag("flags", 0)

    def __repr__(self):
        return '%s(%d, %02x, %d, ...)' % (type(self).__name__,
            self.soundID, self.flags, self.sampleCount, self.data)

@read_tag.when("code == DefineSound.code")
def read_tag_DefineSound(code, length, f, header):
    soundID, flags, sampleCount = struct.unpack("<HBI", f.read(7))
    data = f.read(length - 7)
    return DefineSound(soundID, flags, sampleCount, data)

@serialize_tag.when("isinstance(tag, DefineSound)")
def serialize_tag_DefineSound(tag, header):
    return (
        tag.code,
        struct.pack('<HBI', tag.soundID, tag.flags, tag.sampleCount),
        tag.data,
    )

class StartSound(Value):
    code = 15
    def __init__(self, soundID, syncStop, syncNoMultiple,
            inPoint=None, outPoint=None, loopCount=None, envelope=None):
        self.soundID = soundID
        self.syncStop = syncStop
        self.syncNoMultiple = syncNoMultiple
        self.inPoint = inPoint
        self.outPoint = outPoint
        self.loopCount = loopCount
        self.envelope = envelope

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (self.soundID, self.syncStop,
            self.syncNoMultiple, self.inPoint, self.outPoint, self.loopCount,
            self.envelope))
        
@read_tag.when("code == StartSound.code")
def read_tag_StartSound(code, length, f, header):
    soundID = struct.unpack('<H', f.read(2))[0]
    br = BitReader(f)
    reserved0, reserved1, syncStop, syncNoMultiple, \
        hasEnvelope, hasLoops, hasOutPoint, hasInPoint = br.readbits(8)
    if hasInPoint:
        inPoint = struct.unpack('<I', f.read(4))[0]
    else:
        inPoint = None
    if hasOutPoint:
        outPoint = struct.unpack('<I', f.read(4))[0]
    else:
        outPoint = None
    if hasLoops:
        loopCount = struct.unpack('<H', f.read(2))[0]
    else:
        loopCount = None
    if hasEnvelope:
        envPoints = ord(f.read(1))
        envelope = [
            struct.unpack('<IHH', f.read(8)) for i in xrange(envPoints)
        ]
    else:
        envelope = None
    return StartSound(soundID, syncStop, syncNoMultiple, inPoint, outPoint,
        loopCount, envelope)

@serialize_tag.when("isinstance(tag, StartSound)")
def serialize_tag_StartSound(tag, header):
    fmt = ['<HB']
    ints = [
        tag.soundID,
        msb_toint([
            0, 0, tag.syncStop, tag.syncNoMultiple,
            tag.envelope is not None,
            tag.loopCount is not None,
            tag.outPoint is not None,
            tag.inPoint is not None,
        ])
    ]
    if tag.inPoint is not None:
        fmt.append('I')
        ints.append(tag.inPoint)
    if tag.outPoint is not None:
        fmt.append('I')
        ints.append(tag.outPoint)
    if tag.loopCount is not None:
        fmt.append('H')
        ints.append(tag.loopCount)
    if tag.envelope is not None:
        fmt.append('B')
        ints.append(chr(len(tag.envelope)))
        for point in envelope:
            fmt.append('IHH')
            ints.extend(point)

    return tag.code, struct.pack(''.join(fmt), *ints)

class SoundStreamHead(Value):
    code = 18
    def __init__(self, playbackFlags, flags, sampleCount, latencySeek=None):
        self.playbackFlags = playbackFlags
        self.flags = flags
        self.sampleCount = sampleCount
        self.latencySeek = latencySeek

    playbackRate = bitflag("playbackFlags", 2, 2)
    playbackSize = bitflag("playbackFlags", 1)
    playbackSoundType = bitflag("playbackFlags", 0)
    compression = bitflag("flags", 4, 4)
    rate = bitflag("flags", 2, 2)
    size = bitflag("flags", 1)
    soundType = bitflag("flags", 0)

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (
            self.playbackFlags, self.flags,
            self.sampleCount, self.latencySeek))

@read_tag.when("code == SoundStreamHead.code")
def read_tag_SoundStreamHead(code, length, f, header):
    tag = SoundStreamHead(*struct.unpack('<BBH', f.read(4)))
    if tag.compression == 2:
        tag.latencySeek = struct.unpack('<h', f.read(2))[0]
    return tag

@serialize_tag.when("isinstance(tag, SoundStreamHead)")
def serialize_tag_SoundStreamHead(tag, header):
    fmt = ['<BBH']
    ints = [tag.playbackFlags, tag.flags, tag.sampleCount]
    if tag.latencySeek is not None:
        fmt.append('h')
        ints.append(tag.latencySeek)
    return tag.code, struct.pack(''.join(fmt), *ints)

class SoundStreamHead2(Value):
    code = 45
    def __init__(self, playbackFlags, flags, sampleCount, latencySeek=None):
        self.playbackFlags = playbackFlags
        self.flags = flags
        self.sampleCount = sampleCount
        self.latencySeek = latencySeek

    playbackRate = bitflag("playbackFlags", 2, 2)
    playbackSize = bitflag("playbackFlags", 1)
    playbackSoundType = bitflag("playbackFlags", 0)
    compression = bitflag("flags", 4, 4)
    rate = bitflag("flags", 2, 2)
    size = bitflag("flags", 1)
    soundType = bitflag("flags", 0)

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (
            self.playbackFlags, self.flags,
            self.sampleCount, self.latencySeek))

@read_tag.when("code == SoundStreamHead2.code")
def read_tag_SoundStreamHead2(code, length, f, header):
    tag = SoundStreamHead2(*struct.unpack('<BBH', f.read(4)))
    if tag.compression == 2:
        tag.latencySeek = struct.unpack('<h', f.read(2))[0]
    return tag

@serialize_tag.when("isinstance(tag, SoundStreamHead2)")
def serialize_tag_SoundStreamHead2(tag, header):
    fmt = ['<BBH']
    ints = [tag.playbackFlags, tag.flags, tag.sampleCount]
    if tag.latencySeek is not None:
        fmt.append('h')
        ints.append(tag.latencySeek)
    return tag.code, struct.pack(''.join(fmt), *ints)

class SoundStreamBlock(Value):
    code = 19
    def __init__(self, data):
        self.data = data

    def __repr__(self):
        return '%s(...)' % (type(self).__name__,)
        
@read_tag.when("code == SoundStreamBlock.code")
def read_tag_SoundStreamBlock(code, length, f, header):
    return SoundStreamBlock(f.read(length))

@serialize_tag.when("isinstance(tag, SoundStreamBlock)")
def serialize_tag_SoundStreamBlock(tag, header):
    return tag.code, tag.data

class DefineVideoStream(Value):
    code = 60
    def __init__(self, characterID, numFrames, width, height, flags, codecID):
        self.characterID = characterID
        self.numFrames = numFrames
        self.width = width
        self.height = height
        self.flags = flags
        self.codecID = codecID

    smoothing = bitflag("flags", 0)
    deblocking = bitflag("flags", 1, 2)

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (
            self.characterID, self.numFrames, self.width, self.height,
            self.flags, self.codecID))


@read_tag.when("code == DefineVideoStream.code")
def read_tag_DefineVideoStream(code, length, f, header):
    return DefineVideoStream(*struct.unpack('<HHHHBB', f.read(10)))

@serialize_tag.when("isinstance(tag, DefineVideoStream)")
def serialize_tag_DefineVideoStream(tag, header):
    return (
        tag.code,
        struct.pack(
            '<HHHHBB', tag.characterID, tag.numFrames, tag.width, tag.height,
            tag.flags, tag.codecID,
        ),
    )


class VideoFrame(Value):
    code = 61
    def __init__(self, streamID, frameNum, data):
        self.streamID = streamID
        self.frameNum = frameNum
        self.data = data

    def __repr__(self):
        return '%s(%r, %r, ...)' % (
            type(self).__name__, self.streamID, self.frameNum)

@read_tag.when("code == VideoFrame.code")
def read_tag_VideoFrame(code, length, f, header):
    streamID, frameNum = struct.unpack('<HH', f.read(4))
    return VideoFrame(streamID, frameNum, f.read(length - 4))

@serialize_tag.when("isinstance(tag, VideoFrame)")
def serialize_tag_VideoFrame(tag, header):
    return tag.code, struct.pack('<HH', tag.streamID, tag.frameNum), tag.data

class FileAttributes(Value):
    code = 69
    def __init__(self, flags):
        self.flags = flags

    def __repr__(self):
        return '%s(0x%08x)' % (type(self).__name__, self.flags)

    useNetwork = bitflag("flags", 0)
    hasMetadata = bitflag("flags", 4)

@read_tag.when("code == FileAttributes.code and length == 4")
def read_tag_FileAttributes(code, length, f, header):
    return FileAttributes(struct.unpack("<I", f.read(4))[0])

@serialize_tag.when("isinstance(tag, FileAttributes)")
def serialize_tag(tag, header):
    return tag.code, struct.pack("<I", tag.flags)

class PlaceObject2(Value):
    code = 26
    def __init__(self, move, depth, characterId=None, matrix=None,
            colorTransform=None, ratio=None, name=None, clipDepth=None,
            clipActions=None):
        self.move = move
        self.depth = depth
        self.characterId = characterId
        self.matrix = matrix
        self.colorTransform = colorTransform
        self.ratio = ratio
        self.name = name
        self.clipDepth = clipDepth
        self.clipActions = clipActions

    def __repr__(self):
        return '%s%r' % (type(self).__name__,
            (self.move, self.depth, self.characterId, self.matrix,
            self.colorTransform, self.ratio, self.name, self.clipDepth,
            self.clipActions))
        
@read_tag.when("code == PlaceObject2.code")
def read_tag_PlaceObject2(code, length, f, header):
    br = BitReader(f)
    hasClipActions, hasClipDepth, hasName, \
        hasRatio, hasColorTransform, hasMatrix, \
        hasCharacter, move = br.readbits(8)
    depth = struct.unpack('<H', f.read(2))[0]
    if hasCharacter:
        characterId = struct.unpack('<H', f.read(2))[0]
    else:
        characterId = None
    if hasMatrix:
        matrix = read_matrix(br)
    else:
        matrix = None
    if hasColorTransform:
        colorTransform = read_color_transform_with_alpha(br)
    else:
        colorTransform = None
    if hasRatio:
        ratio = struct.unpack('<H', f.read(2))[0]
    else:
        ratio = None
    if hasName:
        name = read_string(f)
    else:
        name = None
    if hasClipDepth:
        clipDepth = struct.unpack('<H', f.read(2))[0]
    else:
        clipDepth = None
    if hasClipActions:
        clipActions = read_clip_actions(f, header)
    else:
        clipActions = None
    return PlaceObject2(move, depth, characterId, matrix, colorTransform,
        ratio, name, clipDepth, clipActions)

@serialize_tag.when("isinstance(tag, PlaceObject2)")
def serialize_tag_PlaceObject2(tag, header):
    bits = [getattr(tag, x) is not None for x in
        ['clipActions', 'clipDepth', 'name', 'ratio', 'colorTransform',
        'matrix', 'characterId']] + [tag.move]
    yield tag.code
    yield struct.pack('<BH', msb_toint(bits), tag.depth)
    if tag.characterId is not None:
        yield struct.pack('<H', tag.characterId)
    bw = BitWriter()
    if tag.matrix is not None:
        write_matrix(tag.matrix, bw)
    if tag.colorTransform is not None:
        write_color_transform(tag.colorTransform, bw)
    if bw.bits:
        yield bw.tostring()
    if tag.ratio is not None:
        yield struct.pack('<H', tag.ratio)
    if tag.name is not None:
        yield pack_string(f)
    if tag.clipDepth is not None:
        yield struct.pack('<H', tag.clipDepth)
    if tag.clipActions is not None:
        yield serialize_clip_actions(tag.clipActions, header)

class DefineButton2(Value):
    code = 34
    version = 2

    def __init__(self, buttonID, trackAsMenu, characters, actions):
        self.buttonID = buttonID
        self.trackAsMenu = trackAsMenu
        self.characters = characters
        self.actions = actions

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (
            self.buttonID, self.trackAsMenu, self.characters, self.actions))


class ButtonRecord(Value):
    def __init__(self, flags, characterID, placeDepth, placeMatrix, colorTransform=None):
        self.flags = flags
        self.characterID = characterID
        self.placeDepth = placeDepth
        self.placeMatrix = placeMatrix
        self.colorTransform = colorTransform

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (
            self.flags, self.characterID, self.placeDepth, self.placeMatrix,
            self.colorTransform))

def read_button_record(f, ctx):
    flags = ord(f.read(1))
    if not flags:
        return None
    chunk = f.read(4)
    characterID, placeDepth = struct.unpack('<HH', chunk)
    br = BitReader(f)
    placeMatrix = read_matrix(br)
    if ctx.version >= 2:
        colorTransform = read_color_transform_with_alpha(br)
    else:
        colorTransform = None
    return ButtonRecord(flags, characterID, placeDepth, placeMatrix, colorTransform)

def read_button_records(f, ctx, header):
    records = []
    while True:
        record = read_button_record(f, ctx)
        if record is None:
            break
        records.append(record)
    return records

def write_button_record(f, r, ctx, header):
    f.write(struct.pack('<BHH', r.flags, r.characterID, r.placeDepth))
    bw = BitWriter(f)
    write_matrix(r.placeMatrix, bw)
    if ctx.version >= 2:
        write_color_transform_with_alpha(r.colorTransform, bw)
    bw.flush()

def write_button_records(f, records, ctx, header):
    for record in records:
        write_button_record(f, record, ctx, header)
    f.write('\x00')

class ButtonCondAction(Value):
    def __init__(self, conditionBits, keyCode, actions):
        self.conditionBits = conditionBits
        self.keyCode = keyCode
        self.actions = actions

    def __repr__(self):
        return '%s%r' % (type(self).__name__, (
            self.conditionBits, self.keyCode, self.actions))


def read_button_cond_action(f, ctx, header):
    br = BitReader(f)
    conditionBits = br.readbits(8)
    keyCode = br.readUB(7)
    conditionBits.extend(br.readbits(1))
    actions = list(read_actions(f, header))
    return ButtonCondAction(conditionBits, keyCode, actions)

def read_button_cond_actions(f, ctx, header):
    actions = []
    while True:
        actionOffset = struct.unpack('<H', f.read(2))[0]
        actions.append(read_button_cond_action(f, ctx, header))
        if not actionOffset:
            break
    return actions

def write_button_cond_actions(f, actions, ctx, header):
    last = len(actions) - 1
    for i, action in enumerate(actions):
        data = pack_actions(action.actions, header)
        offset = 4 + len(data)
        if i == last:
            offset = 0
        f.write(struct.pack('<H', offset))
        bw = BitWriter()
        bw.writebits(action.conditionBits[:8])
        bw.writeUB(7, action.keyCode)
        bw.writebits(action.conditionBits[8:])
        f.write(bw.tostring())
        f.write(data)

@read_tag.when("code == DefineButton2.code")
def read_tag_DefineButton2(code, length, f, header):
    buttonID, trackAsMenu, actionOffset = struct.unpack('<HBH', f.read(5))
    characters = read_button_records(f, DefineButton2, header)
    if actionOffset:
        actions = read_button_cond_actions(f, DefineButton2, header)
    else:
        actions = []
    return DefineButton2(buttonID, trackAsMenu, characters, actions)

@serialize_tag.when("isinstance(tag, DefineButton2)")
def serialize_tag_DefineButton2(tag, header):
    sio = StringIO()
    write_button_records(sio, tag.characters, DefineButton2, header)
    if tag.actions:
        actionOffset = 2 + sio.tell()
        write_button_cond_actions(sio, tag.actions, DefineButton2, header)
    else:
        actionOffset = 0
    hdr = struct.pack('<HBH', tag.buttonID, tag.trackAsMenu, actionOffset)
    return tag.code, hdr, sio.getvalue()
        

class EndTag(Value):
    code = 0
    def __init__(self):
        return TypeError("singleton")

@read_tag.when("code == EndTag.code and length == 0")
def read_tag_endtag(code, length, f, header):
    return EndTag

@serialize_tag.when("tag is EndTag")
def serialize_tag_EndTag(tag, header):
    return (tag.code,)

class UnknownTag(Value):
    def __init__(self, code, data):
        self.code = code
        self.data = data

    def __repr__(self):
        return '%s(%d, ...)' % (type(self).__name__, self.code)

@read_tag.when(strategy.default)
def read_tag_unknown(code, length, f, header):
    return UnknownTag(code, f.read(length))

@serialize_tag.when("isinstance(tag, UnknownTag)")
def serialize_tag_unknown(tag, header):
    return tag.code, tag.data
    
def pack_tag(tag, header):
    iterable = iter(serialize_tag(tag, header))
    code = iterable.next()
    data = ''.join(list(iter_only(iterable, str)))
    length = len(data)
    if length < 0x3f:
        header = struct.pack('<H', (code << 6) | length)
    else:
        header = struct.pack('<HI', (code << 6) | 0x3f, length)
    return header + data

def pack_tags(tags, header):
    for tag in tags:
        yield pack_tag(tag, header)
    yield pack_tag(EndTag, header)

def write_tags(tags, f, header):
    writelines(f, pack_tags(tags, header))
 
def write_swf(header, tags, f):
    didWrap = not hasattr(f, 'seek')
    didCompress = header.signature.compressed
    version = header.signature.version
    if didCompress:
        if version < 6:
            raise ValueError("SWF version %d can not be compressed" %
                (version,))
        signature = 'CWS'
    else:
        signature = 'FWS'
    if didWrap:
        hf = StringIO()
    else:
        hf = f
    hf.write(struct.pack('<3sBI', signature, version, 0xDEADBEEF))
    if didCompress:
        cf = ZlibWriter(hf)
    else:
        cf = hf
    write_header(header, cf)
    write_tags(tags, cf, header)
    size = cf.tell()
    if didCompress:
        cf.close()
    end = hf.tell()
    hf.seek(4)
    hf.write(struct.pack('<I', size))
    hf.seek(end)
    if didWrap:
        shutil.copyfileobj(hf, f)

def read_body(f, header):
    while True:
        code, length = read_tagheader(f)
        tag = read_tag(code, length, f, header)
        if tag is None:
            print (code, length)
        if tag is EndTag:
            break
        yield tag
