from flashticle.util import Value
SIMPLEACTIONS = {}
SIMPLEACTIONS_CODE = {}

_SIMPLEACTIONS = """
17  Pop
0A  Add
0B  Subtract
0C  Multiply
0D  Divide
0E  Equals
0F  Less
10  And
11  Or
12  Not
13  StringEquals
14  StringLength
21  StringAdd
15  StringExtract
29  StringLess
31  MBStringLength
35  MBStringExtract
18  ToInteger
32  CharToAscii
33  AsciiToChar
36  MBCharToAscii
37  MBAsciiToChar
1C  GetVariable
1D  SetVariable
20  SetTarget2
22  GetProperty
23  SetProperty
24  CloneSprite
25  RemoveSprite
27  StartDrag
28  EndDrag
26  Trace
34  GetTime
30  RandomNumber
3D  CallFunction
52  CallMethod
3C  DefineLocal
41  DefineLocal2
3A  Delete
3B  Delete2
46  Enumerate
49  Equals2
4E  GetMember
42  InitArray
43  InitObject
53  NewMethod
40  NewObject
4F  SetMember
45  TargetPath
4A  ToNumber
4B  ToString
44  TypeOf
47  Add2
48  Less2
3F  Modulo
60  BitAnd
63  BitLShift
61  BitOr
64  BitRShift
65  BitURShift
62  BitXor
51  Decrement
50  Increment
4C  PushDuplicate
3E  Return
4D  StackSwap
54  InstanceOf
55  Enumerate2
66  StrictEquals
67  Greater
68  StringGreater
69  Extends
2B  CastOp
2C  ImplementsOp
2A  Throw
"""

def _init():
    for line in _SIMPLEACTIONS.splitlines():
        line = line.strip()
        if not line:
            continue
        code, name = line.split()
        code = int(code, 16)
        name = 'Action' + name
        cls = type(name, (Value,), {'code': code, '__module__': __name__})
        SIMPLEACTIONS[name] = cls
        SIMPLEACTIONS_CODE[code] = cls
    globals().update(SIMPLEACTIONS)
_init()
