import struct
from itertools import starmap, count

from flashticle import amf
from flashticle.amf import TypeCodes, from_amf, write_amf, typedobject
from flashticle.util import Value, StringIO, iter_only, writelines, pack_utf8

import logging
log = logging.getLogger( "flashticle.remoting" )
import time

class Request(Value):
    def __init__(self, clientType, raw_headers, raw_body):
        self.clientType = clientType
        self.raw_headers = raw_headers
        self.raw_body = raw_body
        self.headers = dict((k, v) for (k, ig, v) in raw_headers)

    def __repr__(self):
        return '%s%r' % (type(self).__name__,
            (self.clientType, self.raw_headers, self.raw_body))

def _read_header_element(f):
    name = from_amf(TypeCodes.UTF8, f)
    required = from_amf(TypeCodes.BOOL, f)
    length = struct.unpack('>I', f.read(4))[0]
    typ = f.read(1)
    value = from_amf(typ, f)
    # TODO: verify length?
    return name, required, value

def _to_header_element(name, required, value):
    io = StringIO()
    write_amf(value, io)
    return pack_utf8(name) + (required and '\x00' or '\x01') + \
        struct.pack('>I', io.tell()) + io.getvalue()

def _read_body_element(f):
    target = from_amf(TypeCodes.UTF8, f)
    response = from_amf(TypeCodes.UTF8, f)
    length = struct.unpack('>I', f.read(4))[0]
    typ = f.read(1)
    value = from_amf(typ, f)
    # TODO: verify length/
    return target, response, value

def _to_body_element(target, response, value):
    io = StringIO()
    write_amf(value, io)
    return pack_utf8(target) + pack_utf8(response) + \
        struct.pack('>I', io.tell()) + io.getvalue()

def from_remoting(f):
    firstByte, clientType = map(ord, f.read(2))
    if firstByte not in (0, 3):
        raise ValueError("Not expecting first byte: %02x" % (firstByte,))
    header_cnt = struct.unpack('>H', f.read(2))[0]
    headers = [_read_header_element(f) for i in xrange(header_cnt)]
    body_cnt = struct.unpack('>H', f.read(2))[0]
    body = [_read_body_element(f) for i in xrange(body_cnt)]
    return Request(clientType, headers, body)

def to_remoting(headers, body, f):
    log.debug( "to_remoting started" )
    start = time.time()
    f.write(struct.pack('>HH', 0, len(headers)))
    toheaders = (_to_header_element(*elem) for elem in headers)
    writelines(f, iter_only(toheaders, str))

    f.write(struct.pack('>H', len(body)))
    tobody = (_to_body_element(*elem) for elem in body)
    writelines(f, iter_only(tobody, str))
    finish = time.time()
    log.debug( "to_remoting took %.3f" % (finish - start) )

def getIdentifier(ctr=count(1)):
    return ctr.next()

def recordset(colnames, rows, identifier=None):
    try:
        count = len(rows)
    except TypeError:
        rows = list(rows)
        count = len(rows)
    if identifier is None:
        identifier = 'RecordSet:' + str(getIdentifier())
    return typedobject("RecordSet", {
        'serverInfo': {
            'totalCount': count,
            'initialData': rows,
            'cursor': 1,
            'serviceName': 'PageAbleResult',
            'columnNames': colnames,
            'version': 1,
            'id': identifier,
        },
    })


