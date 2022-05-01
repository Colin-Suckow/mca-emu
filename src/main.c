#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>


typedef struct Registers {
    uint32_t pc;
    uint32_t grs[32];
} Registers;

typedef struct TestMachine {
    Registers regs;
    uint8_t *mem;
    uint32_t mem_len;
} TestMachine;

typedef enum {
    NOP = 0,
    JMP,
    LW,
    SW,
    LHI,
    LUHI,
    BNE,
    MV,
    ADD,
    BRK,
} Opcode;

typedef struct Instruction {
    Opcode opcode;
    uint8_t ra1;
    uint8_t ra2;
    uint8_t ra3;
    int16_t offset;
} Instruction;

TestMachine* TestMachine_new(uint32_t memory_size) {
    TestMachine* machine = malloc(sizeof(TestMachine));
    uint8_t *memory = malloc(sizeof(uint8_t) * memory_size);
    machine->regs.pc = 0x800; //Entrypoint
    machine->mem_len = memory_size;
    machine->mem = memory;
    return machine;
}


void TestMachine_destroy(TestMachine* machine) {
    free(machine->mem);
    free(machine);
}

void TestMachine_print_stats(TestMachine* machine) {
    printf("%d bytes of memory\n", machine->mem_len);
    printf("Registers\n");
    printf("PC: 0x%x\n", machine->regs.pc);
    for(int i = 0; i < 32; i++) {
        printf("r%u: 0x%x\n", i,  machine->regs.grs[i]);
    }
}

Instruction decode_instruction_word(uint32_t word) {
    Instruction result;
    result.opcode = (Opcode) word >> 26;
    result.ra1 = (uint8_t) (word >> 21) & 0x1F;
    result.ra2 = (uint8_t) (word >> 16) & 0x1F;
    result.ra3 = (uint8_t) word & 0xFF;
    result.offset = (uint16_t) word & 0xFFFF;
    return result;
}

const char* opcode_name(Opcode opcode) {
    switch(opcode) {
        case NOP: return "NOP";
        case JMP: return "JMP";
        case LW: return "LW";
        case SW: return "SW";
        case LHI: return "LHI";
        case LUHI: return "LUHI";
        case BNE: return "BNE";
        case MV: return "MV";
        case ADD: return "ADD";
        case BRK: return "BRK";
        default: return "UNKNOWN";
    }
}

void instruction_print(Instruction* instr) {
    printf("opcode: %s\n", opcode_name(instr->opcode));
    printf("ra1: 0x%x\n", instr->ra1);
    printf("ra2: 0x%x\n", instr->ra2);
    printf("ra3: 0x%x\n", instr->ra3);
    printf("offset: 0x%x (%d)\n", instr->offset, instr->offset);
}

enum execution_error {
    SUCCESS = 0,
    UNKNOWN_OPCODE,
    ADDRESS_ALIGNMENT,
    BREAKPOINT_HIT,
};

void cpu_exception_print(enum execution_error error, Instruction* instr) {
    printf("EXCEPTION: ");
    switch(error) {
        case UNKNOWN_OPCODE:
            printf("Unknown opcode 0x%x!", instr->opcode);
            break;
        case ADDRESS_ALIGNMENT:
            printf("Unaligned target address!");
            break;
        case BREAKPOINT_HIT:
            printf("Breakpoint hit!\n");
            break;
        default:
            printf("Unknown exception! Did you forget to return SUCCESS from an op handler?");
            break;
    };
}

enum execution_error op_jmp(TestMachine* machine, Instruction* instr) {
    uint32_t target;
    if(instr->offset < 0) {
        target = machine->regs.grs[instr->ra1] - ((uint32_t) instr->offset);
    } else {
        target = machine->regs.grs[instr->ra1] + ((uint32_t) instr->offset);
    }
    if(target % 4 != 0) {
        return ADDRESS_ALIGNMENT;
    }
    machine->regs.pc = target;
    return SUCCESS;
}

enum execution_error op_lw(TestMachine* machine, Instruction* instr) {
    uint32_t addr = machine->regs.grs[instr->ra1];
    if(addr + 4 > machine->mem_len) {
        // Invalid memory address, but no exception should be fired
        // Just load 0 instead
        machine->regs.grs[instr->ra2] = 0;
        return SUCCESS;
    }
    uint32_t value = ((uint32_t) machine->mem[addr]) | ((uint32_t) machine->mem[addr + 1] << 8) | ((uint32_t) machine->mem[addr + 2] << 16) | ((uint32_t) machine->mem[addr + 3] << 24);
    machine->regs.grs[instr->ra2] = value;
    return SUCCESS;
}

enum execution_error op_sw(TestMachine* machine, Instruction* instr) {
    uint32_t addr = machine->regs.grs[instr->ra2];
    uint32_t val = machine->regs.grs[instr->ra1];
    if(addr + 4 > machine->mem_len) {
        // Invalid memory address, but no exception should be fired
        // Just do nothing
        return SUCCESS;
    }
    machine->mem[addr] = (uint8_t) (val & 0xFF);
    machine->mem[addr + 1] = (uint8_t) ((val >> 8) & 0xFF);
    machine->mem[addr + 2] = (uint8_t) ((val >> 16) & 0xFF);
    machine->mem[addr + 3] = (uint8_t) ((val >> 24) & 0xFF);
    return SUCCESS;
}

enum execution_error op_lhi(TestMachine* machine, Instruction* instr) {
    uint32_t upper = machine->regs.grs[instr->ra1] & 0xFFFF0000;
    machine->regs.grs[instr->ra1] = upper | (uint32_t) instr->offset & 0xFFFF;
    return SUCCESS;
}

enum execution_error op_luhi(TestMachine* machine, Instruction* instr) {
    uint32_t lower = machine->regs.grs[instr->ra1] & 0xFFFF;
    machine->regs.grs[instr->ra1] =  ((uint32_t) instr->offset & 0xFFFF) << 16 | lower;
    return SUCCESS;
}

enum execution_error op_bne(TestMachine* machine, Instruction* instr) {
    if(machine->regs.grs[instr->ra1] != machine->regs.grs[instr->ra2]) {
        if(machine->regs.grs[instr->ra2] % 4 != 0) {
            return ADDRESS_ALIGNMENT;
        } else {
            machine->regs.pc += instr->offset;
        }
    }
    return SUCCESS;
}

enum execution_error op_mv(TestMachine* machine, Instruction* instr) {
    machine->regs.grs[instr->ra2] = machine->regs.grs[instr->ra1];
    return SUCCESS;
}

enum execution_error op_add(TestMachine* machine, Instruction* instr) {
    machine->regs.grs[instr->ra3] = machine->regs.grs[instr->ra1] + machine->regs.grs[instr->ra2];
    return SUCCESS;
}

enum execution_error TestMachine_execute(TestMachine* machine, Instruction* instr) {
    uint32_t addr;
    switch(instr->opcode) {
        case NOP: return SUCCESS;
        case JMP: return op_jmp(machine, instr);
        case LW: return op_lw(machine, instr);
        case SW: return op_sw(machine, instr);
        case LHI: return op_lhi(machine, instr);
        case LUHI: return op_luhi(machine, instr);
        case BNE: return op_bne(machine, instr);
        case MV: return op_mv(machine, instr);
        case ADD: return op_add(machine, instr);
        case BRK: return BREAKPOINT_HIT;
        default: return UNKNOWN_OPCODE;
    }
}
void TestMachine_load_binary(TestMachine* machine) {
    FILE* file_ptr;
    file_ptr = fopen("./out.bin", "rb");
    fseek(file_ptr, 0L, SEEK_END);
    int size = ftell(file_ptr);
    rewind(file_ptr);
    printf("size %i\n", size);
    // TODO: Check file length before writing to make sure it fits in memory
    fread(machine->mem + 0x800, size, 1, file_ptr);
    close(file_ptr);
}

int main(int argc, char **argv) {
    TestMachine* machine = TestMachine_new(1048576);
    printf("TestMachine initialized!\n");

    TestMachine_load_binary(machine);

    enum execution_error error;
    while(1) {
        uint32_t instr_word = ((uint32_t) machine->mem[machine->regs.pc]) | ((uint32_t) machine->mem[machine->regs.pc + 1] << 8) | ((uint32_t) machine->mem[machine->regs.pc + 2] << 16) | ((uint32_t) machine->mem[machine->regs.pc + 3] << 24);
        Instruction instruction = decode_instruction_word(instr_word);
        if((error = TestMachine_execute(machine, &instruction)) != 0) {
            cpu_exception_print(error, &instruction);
            printf("\n");
            TestMachine_print_stats(machine);
            printf(" Exiting...\n");
            TestMachine_destroy(machine);
            return -1;
        }
        machine->regs.pc += 4;
    }
    return 0;
}