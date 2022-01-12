import struct
import shutil

import dispatch
from dispatch import strategy, functions, combiners, NoApplicableMethods

from flashticle import amf
from flashticle.util import check, bitflag, iter_only, StringIO
from flashticle.util import msb_fromstring, msb_toint, Value, BitReader

SOUNDFORMATS = [
    'Uncompressed', 'ADPCM', 'MP3',
    'RESERVED', 'RESERVED',
    'Nellymoser 8 kHz mono', 'Nellymoser',
]
SOUNDRATES = ['5.5 kHz', '11 kHz', '22 kHz', '44 kHz']
SOUNDSIZES = ['8-bit', '16-bit']
SOUNDTYPES = ['mono', 'stereo']
CODECIDS = {
    2: 'Sorensen H.263',
    3: 'Screen video',
    4: 'On2 VP6',
    5: 'On2 VP6 (2)',
}
FRAMETYPES = {1: 'keyframe', 2: 'inter frame', 3: 'disposable inter frame'}

class HeaderFlags:
    audio = 4
    video = 1

class Codecs:
    H263 = 2
    SCREENVIDEO = 3
    VP6 = 4
    VP6_2 = 5

class FrameTypes:
    keyframe = 1
    interFrame = 2
    disposableInterFrame = 3

class TypeCodes:
    AUDIO = '\x08'
    VIDEO = '\x09'
    META = '\x12'
    UNDEFINED = '\x00'

class Header_v1(Value):
    def __init__(self, flags=0):
        self.flags = flags

    audio = bitflag('flags', 2)
    video = bitflag('flags', 0)

    @dispatch.generic()
    def to_file(self, f, version):
        pass

    def __repr__(self):
        return '%s(%r)' % (type(self).__name__, self.flags)

    def __str__(self):
        flags = [x for x in ['audio', 'video'] if getattr(self, x)]
        flags.insert(0, type(self).__name__)
        return '<%s>' % (' '.join(flags),)

@Header_v1.to_file.when("version == 1")
def Header_v1_to_file_v1(self, f, version):
    f.write('FLV\x01' + chr(self.flags) + '\x00\x00\x00\x09')

def _tag_header(code, size, timestamp, f):
    f.write(code)
    f.write(struct.pack('>I', size)[1:])
    f.write(struct.pack('>II', timestamp, 0)[1:])

class Meta(Value):
    def __init__(self, timestamp, event, data):
        self.timestamp = timestamp
        self.event = event
        self.data = data

    @dispatch.generic()
    def to_file(self, f, header):
        pass

    def __repr__(self):
        return '%s%r' % (
            type(self).__name__, (self.timestamp, self.event, self.data))

@Meta.to_file.when("isinstance(header, Header_v1)")
def Meta_to_file_v1(self, f, header):
    sio = StringIO()
    amf.write_amf(self.event, sio)
    amf.write_amf(self.data, sio)
    data = sio.getvalue()
    _tag_header(TypeCodes.META, len(data), self.timestamp, f)
    f.write(data)

class Audio(Value):
    def __init__(self, timestamp, flags, data):
        self.timestamp = timestamp
        self.flags = flags
        self.data = data

    @dispatch.generic()
    def to_file(self, f, header):
        pass

    soundType = bitflag("flags", 0, 1)
    soundSize = bitflag("flags", 1, 1)
    soundRate = bitflag("flags", 2, 2)
    soundFormat = bitflag("flags", 4, 4)

    def __repr__(self):
        return '%s(%r, %r, ...)' % (
            type(self).__name__, self.timestamp, self.flags)

    @property
    def description(self):
        if self.soundFormat == 5:
            return SOUNDFORMATS[5]
        return ' '.join((
            SOUNDFORMATS[self.soundFormat],
            SOUNDRATES[self.soundRate],
            SOUNDSIZES[self.soundSize],
            SOUNDTYPES[self.soundType],
        ))
    
    def __str__(self):
        return '<%s %dms %s>' % (
            type(self).__name__,
            self.timestamp,
            self.description,
        )

@Audio.to_file.when("isinstance(header, Header_v1)")
def Audio_to_file_v1(self, f, header):
    _tag_header(TypeCodes.AUDIO, 1 + len(self.data), self.timestamp, f)
    f.write(chr(self.flags))
    f.write(self.data)

class Video(Value):
    def __init__(self, timestamp, flags, data):
        self.timestamp = timestamp
        self.flags = flags
        self.data = data

    @dispatch.generic()
    def get_resolution(self):
        pass

    @dispatch.generic()
    def to_file(self, f, header):
        pass

    codecID = bitflag("flags", 0, 4)
    frameType = bitflag("flags", 4, 4)

    def __repr__(self):
        return '%s(%r, %r, ...[%d])' % (
            type(self).__name__, self.timestamp, self.flags, len(self.data))

    @property
    def codecName(self):
        return CODECIDS.get(
            self.codecID, "CODEC[%d]" % (self.codecID))

    @property
    def frameTypeName(self):
        return FRAMETYPES.get(
            self.frameType, "FRAMETYPE[%d]" % (self.frameType))
        
    def __str__(self):
        w, h = self.get_resolution()
        if w is not None and h is not None:
            res = ' %dx%d' % (w, h)
        else:
            res = ''
        return '<%s %dms %s %s%s>' % (
            type(self).__name__,
            self.timestamp,
            self.codecName,
            self.frameTypeName,
            res,
        )

SORENSEN_RES = {
    2: (352, 288),  # CIF
    3: (176, 144),  # QCIF
    4: (128, 96),   # SQCIF
    5: (320, 240),
    6: (160, 120),
}

@Video.get_resolution.when(strategy.default)
def Video_get_resolution_default(self):
    return None, None

# @Video.get_resolution.when("self.codecID == Codecs.VP6")
#
# TODO: http://wiki.multimedia.cx/index.php?title=On2_VP6

@Video.get_resolution.when("self.codecID == Codecs.H263")
def Video_get_resolution_Sorensen(self):
    # H263VIDEOPACKET
    br = BitReader(StringIO(self.data))
    # expect: 1
    pictureStartCode = br.readUB(17)
    # expect: 0 or 1
    version = br.readUB(5)
    temporalReference = br.readUB(8)
    pictureSize = br.readUB(3)
    if pictureSize == 0:
        res = br.readUB(8), br.readUB(8)
    elif pictureSize == 1:
        res = br.readUB(16), br.readUB(16)
    else:
        res = SORENSEN_RES[pictureSize]
    return res

@Video.get_resolution.when("self.codecID == Codecs.SCREENVIDEO")
def Video_get_resolution_ScreenVideo(self):
    width, height = struct.unpack('>HH', self.data[:4])
    return width & 0xfff, height & 0xfff

@Video.to_file.when("isinstance(header, Header_v1)")
def Video_to_file_v1(self, f, header):
    _tag_header(TypeCodes.VIDEO, 1 + len(self.data), self.timestamp, f)
    f.write(chr(self.flags))
    f.write(self.data)

@dispatch.generic()
def header(version, flags, offset):
    pass

@header.when("version == 1 and offset == 9")
def header_v1(version, flags, offset):
    return Header_v1(flags)

@header.when(strategy.default)
def header_default(version, flags, offset):
    raise ValueError("FLV version %r offset %r not supported"
        % (version, offset))

def read_header(f):
    check('FLV', f, 'Incorrect signature')
    version = ord(f.read(1))
    flags = ord(f.read(1))
    offset = struct.unpack('>I', f.read(4))[0]
    return header(version, flags, offset)

def read_body(f):
    tag = None
    tagSize = 0
    while True:
        previousTagSize = struct.unpack('>I', f.read(4))[0]
        if previousTagSize != tagSize:
            raise ValueError("tag size mismatch %r != %r"
                % (previousTagSize, tagSize))
        try:
            tag, tagSize = read_tag(f)
        except EOFError:
            break
        yield tag

def read_tag(f):
    typ = f.read(1)
    if len(typ) == 0:
        raise EOFError
    data_size = struct.unpack('>I', '\x00' + f.read(3))[0]
    timestamp, reserved = struct.unpack('>II', '\x00' + f.read(7))
    data = f.read(data_size)
    return tag(typ, timestamp, data, reserved), 11 + len(data)

@dispatch.generic()
def tag(typ, timestamp, data, reserved=0):
    pass

@tag.when(strategy.default)
def tag_default(typ, timestamp, data, reserved=0):
    raise ValueError("Unknown tag: type=%r timestamp=%r data=%r, reserved=%r"
        % (typ, timestamp, data, reserved))

@tag.when("typ == TypeCodes.AUDIO and reserved == 0")
def tag_audio(typ, timestamp, data, reserved=0):
    return Audio(timestamp, ord(data[0]), data[1:])

@tag.when("typ == TypeCodes.VIDEO and reserved == 0")
def tag_video(typ, timestamp, data, reserved=0):
    return Video(timestamp, ord(data[0]), data[1:])

@tag.when("typ == TypeCodes.META and reserved == 0")
def tag_meta(typ, timestamp, data, reserved=0):
    sio = StringIO(data)
    event = amf.from_amf(sio.read(1), sio)
    meta = amf.from_amf(sio.read(1), sio)
    return Meta(timestamp, event, meta)

@dispatch.generic()
def write_header(header, f, version):
    pass

@write_header.when("hasattr(header, 'to_file')")
def write_header_to_file(header, f, version):
    header.to_file(f, version)

@dispatch.generic()
def write_tag(tag, f, header):
    pass

@write_tag.when("hasattr(tag, 'to_file')")
def write_tag_flv(tag, f, header):
    tag.to_file(f, header)

def from_flv(f):
    return read_header(f), read_body(f)

def write_flv(header, tags, f, version=1):
    write_header(header, f, version)
    f.write('\x00\x00\x00\x00')
    for tag in tags:
        pos = f.tell()
        write_tag(tag, f, header)
        tagSize = f.tell() - pos
        f.write(struct.pack('>I', tagSize))
