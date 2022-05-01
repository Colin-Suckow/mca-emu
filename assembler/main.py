import sys
from typing import Any, Optional

class Register:
    def __init__(self, reg: int) -> None:
        self.reg = reg

    def as_ra1(self) -> int:
        return self.reg << 21

    def as_ra2(self) -> int:
        return self.reg << 16

    def as_ra3(self) -> int:
        return self.reg

    def __str__(self) -> str:
        return "r{}".format(self.reg)

def translate_argument(arg: str) -> Any:
    if arg[0] == "$":
        # Constant number
        return int(arg[1::], base=16)
    elif arg[0] == "r":
        return Register(int(arg[1::]))
    elif arg[0] == ".":
        addr = labels[arg]
        if addr is None:
            print(f"Unknown label {arg}")
            raise Exception
        return addr
    else:
        return arg

opcode_encodings = {
    "nop": 0x0,
    "jmp": 0x1,
    "lw": 0x2,
    "sw": 0x3,
    "lhi": 0x4,
    "luhi": 0x5,
    "bne": 0x6,
    "mv": 0x7,
    "add": 0x8,
    "brk": 0x9,
}

current_addr = 0x800
labels = {}


class IRInstruction:
    def __init__(self, operation: str, arguments: list[Any]):
        self.operation = operation.lower().strip()
        self.arguments = arguments
    
    @staticmethod
    def parse(line: str):
        parts = list(map(str.strip, line.split(' ')))
        return IRInstruction(parts[0], list(map(translate_argument, parts[1::])))

    def apply_arguments(self, instr: int) -> int:
        if self.operation == "jmp" or self.operation == "lhi" or self.operation == "luhi":
            return instr | self.arguments[0].as_ra1() | self.arguments[1]
        elif self.operation == "lw" or self.operation == "sw" or self.operation == "mv":
            return instr | self.arguments[0].as_ra2() | self.arguments[1].as_ra1()
        elif self.operation == "add":
            return instr | self.arguments[0].as_ra3() | self.arguments[1].as_ra1() | self.arguments[2].as_ra2()
        elif self.operation == "bne":
            target = (self.arguments[0] - current_addr) & 0xFFFF
            print(f'label {self.arguments[0]} current {current_addr}')
            return instr | target | self.arguments[1].as_ra1() | self.arguments[2].as_ra2()
        else:
            return instr

    def encode(self) -> int:
        instr = opcode_encodings[self.operation] << 26
        return self.apply_arguments(instr)
        
    def __str__(self) -> str:
        return "{} {}".format(self.operation, self.arguments)

def parse(line) -> Optional[IRInstruction]:
    parts = list(map(str.strip, line.split(' ')))
    if parts[0][0] == '.':
        if parts[0] in labels:
            print(f"Label {parts[0]} already defined!")
            raise Exception
        else:
            labels[parts[0]] = current_addr
            print(f'Added label {parts[0]}')
            return None
    else:
        return IRInstruction.parse(line)


def main():
    global current_addr
    if len(sys.argv) < 2:
        print('Missing input file')
        return

    with open(sys.argv[1], 'r') as source:
        source_lines = source.readlines()
        instrs: bytearray = bytearray()
        for line in source_lines:
            instr = parse(line.strip())
            if instr is not None:
                print(f"Adding instr {instr}")
                instrs += bytearray(instr.encode().to_bytes(4, byteorder="little"))
            current_addr += 4
        with open("out.bin", "wb") as out_file:
            out_file.write(instrs)


if __name__ == "__main__":
    main()