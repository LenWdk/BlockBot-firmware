#!/usr/bin/env python3

from decoders import *

class DecodeExeCollection(Exception):
    def __init__(self, linenum=None):
        self.linenum= linenum
        self.errors=[]

    def append(self, exception):
        self.errors.append(exception)
    
    def __str__(self):
        errs= 'Error parsing line {} '.format(self.linenum)
        errs+='see below for errors the separate parsing modules produced\n'

        errs+= '\n'.join(str(e) for e in sorted(self.errors))

        return (errs)

    
class CmdConstant (ProtoConstant):
    def parse(self, line):
        try:
            return int(line, base=0)
        except ValueError:
            errtxt= '{} is not a number'.format(line)
            raise DecodeException('low', errtxt)

class Program(object):
    decoders= [
        CmdSOV, CmdSPU, CmdSPO, CmdSPJ,
        CmdLD,
        CmdDEC, CmdINC, CmdNOT, CmdSRR,
        CmdJFW, CmdJBW,
        CmdMOV, CmdOR,  CmdAND, CmdXOR,
        CmdADD, CmdSUB, CmdSEQ, CmdSNE,
        CmdLDA, CmdSTA,
        AliasNEG, AliasSRL, AliasNOP,
        CmdLabel,
        CmdConstant
    ]
    
    def __init__(self):
        self.labels={}
        self.ops=[]

    def _lines_from_text(self, text):
        lines= text.split('\n')

        def rm_comment(ln):
            sep= ln.find('//')

            if sep >= 0:
                ln= ln[:sep]

            return (ln.strip())

        def not_empty(ln):
            return (ln if ln != '' else False)

        lines= list(map(rm_comment, lines))
        lines= list(filter(not_empty, lines))

        return (lines)

    def _decode_line(self, line):
        linenum, ln= line
        
        exe= DecodeExeCollection(linenum)
        
        for dc in self.decoders:
            try:
                return (dc(self, ln))
            except DecodeException as e:
                exe.append(e)

        raise exe
    
    def from_text(self, text):
        lines= self._lines_from_text(text)

        self.ops.extend(map(self._decode_line, enumerate(lines)))

        offset=0
        for op in self.ops:
            op.place(offset)
            offset+=len(op)

    def from_textfile(self, path):
        fd= open(path, encoding='utf-8')
        text=fd.read()
        fd.close()

        self.from_text(text)
        
    def from_bytecode(self, bc):
        pass

    def to_text(self):
        return ('\n'.join(map(str, self.ops)))

    def to_bytecode(self):
        bc= bytes()

        for op in self.ops:
            bc+=bytes(op)

        return bc

p=Program()
p.from_textfile('tst.brkas')
