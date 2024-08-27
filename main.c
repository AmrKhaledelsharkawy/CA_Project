#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Define constants
#define INSTRUCTION_MEMORY_SIZE 1024 // 16-bit words
#define DATA_MEMORY_SIZE 2048        // 8-bit bytes
#define REGISTER_COUNT 66

// Define flags in the status register
#define CARRY_FLAG 0x01
#define OVERFLOW_FLAG 0x02
#define NEGATIVE_FLAG 0x04
#define SIGN_FLAG 0x08
#define ZERO_FLAG 0x10

// Define structures for the pipeline stages
typedef struct {
    uint16_t instruction;
    int active;
} PipelineStage;

typedef struct 
{
    short int instruction
}IFID;

typedef struct 
{
    int opcode;
    uint8_t rd;
    uint8_t rs1;
    uint8_t immediate;
}IDEX;


typedef struct { // instruction will be short int (16 bits) -- PC will be int (32 bits)
    uint16_t instruction_memory[INSTRUCTION_MEMORY_SIZE];
    uint8_t data_memory[DATA_MEMORY_SIZE];
    uint8_t registers[REGISTER_COUNT];
    uint16_t pc;
    PipelineStage fetch, decode, execute;
    uint8_t sreg; // Status Register
    //fetchedInstruction CurrentInstruction;
   // DecodedInstruction IR;
    IFID IFID;
    IDEX IDEX;
    int instruction_count;
} CPU;



typedef struct {
    short int instruction;
    
} fetchedInstruction;
typedef struct {
    uint8_t opcode;
    uint8_t rd;
    uint8_t rs1;
    uint8_t immediate;
} DecodedInstruction;


// Function prototypes
void initialize_cpu(CPU *cpu); // MALAK 
void load_program(CPU *cpu, const char *filename); // Fares
void print_cpu_state(CPU *cpu); // Fares
void run_pipeline(CPU *cpu); // marwan amr 
void fetch(CPU *cpu); //Noor to to use the PC to acces the instruction memory and fetch the instruction into the current instruction register
void decode(CPU *cpu);//Noor to decode the instruction in the current instruction register and store the result in the IR
void execute(CPU *cpu);
DecodedInstruction decode_instruction(uint16_t instruction);
void update_flags(CPU *cpu, uint8_t result);
void execute_add(CPU *cpu, DecodedInstruction instr);
void execute_sub(CPU *cpu, DecodedInstruction instr);
void execute_mul(CPU *cpu, DecodedInstruction instr);
void execute_movi(CPU *cpu, DecodedInstruction instr);
void execute_beqz(CPU *cpu, DecodedInstruction instr);
void execute_andi(CPU *cpu, DecodedInstruction instr);
void execute_eor(CPU *cpu, DecodedInstruction instr);
void execute_br(CPU *cpu, DecodedInstruction instr);
void execute_sal(CPU *cpu, DecodedInstruction instr);
void execute_sar(CPU *cpu, DecodedInstruction instr);
void execute_ldr(CPU *cpu, DecodedInstruction instr);
void execute_str(CPU *cpu, DecodedInstruction instr);

int main() {
    CPU cpu;
    initialize_cpu(&cpu);
    load_program(&cpu, "program.txt"); // Replace with your program file
    run_pipeline(&cpu);
    return 0;
}

// Empty function implementations for milestone one
void initialize_cpu(CPU *cpu) {
    for(int i = 0 ; i < INSTRUCTION_MEMORY_SIZE ; i++){
        cpu->instruction_memory[i] = 0;
    }
    
    for(int i = 0 ; i < DATA_MEMORY_SIZE ; i++){
        cpu->data_memory[i] = 0;
    }
    
    for(int i = 0 ; i < REGISTER_COUNT ; i++){
        cpu->registers[i] = 0;
    }
    cpu->pc= 0; 
    cpu->IFID.instruction=0;
    cpu->IDEX.immediate=0;
    cpu->IDEX.opcode=0;
    cpu->IDEX.rd=0;
    cpu->IDEX.rs1=0;

}

void load_program(CPU *cpu, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: Unable to open file %s\n", filename);
        return;
    }

    char line[256];
    int instruction_index = 0;

    while (fgets(line, sizeof(line), file)) {
        if (instruction_index >= INSTRUCTION_MEMORY_SIZE) {
            printf("Error: Program too large to fit in instruction memory.\n");
            break;
        }

        char opcode[10];
        uint8_t rd, rs1, immediate;
        uint16_t binary_instruction = 0;

        // Parse each instruction based on its format
        if (sscanf(line, "ADD R%hhu R%hhu", &rd, &rs1) == 2) {
            binary_instruction = (0x00 << 12) | (rd << 8) | (rs1 << 4);
        } else if (sscanf(line, "SUB R%hhu R%hhu", &rd, &rs1) == 2) {
            binary_instruction = (0x01 << 12) | (rd << 8) | (rs1 << 4);
        } else if (sscanf(line, "MUL R%hhu R%hhu", &rd, &rs1) == 2) {
            binary_instruction = (0x02 << 12) | (rd << 8) | (rs1 << 4);
        } else if (sscanf(line, "MOVI R%hhu %hhu", &rd, &immediate) == 2) {
            binary_instruction = (0x03 << 12) | (rd << 8) | immediate;
        } else if (sscanf(line, "BEQZ R%hhu %hhu", &rd, &immediate) == 2) {
            binary_instruction = (0x04 << 12) | (rd << 8) | immediate;
        } else if (sscanf(line, "ANDI R%hhu %hhu", &rd, &immediate) == 2) {
            binary_instruction = (0x05 << 12) | (rd << 8) | immediate;
        } else if (sscanf(line, "EOR R%hhu R%hhu", &rd, &rs1) == 2) {
            binary_instruction = (0x06 << 12) | (rd << 8) | (rs1 << 4);
        } else if (sscanf(line, "BR R%hhu", &rd) == 1) {
            binary_instruction = (0x07 << 12) | (rd << 8);
        } else if (sscanf(line, "SAL R%hhu %hhu", &rd, &immediate) == 2) {
            binary_instruction = (0x08 << 12) | (rd << 8) | immediate;
        } else if (sscanf(line, "SAR R%hhu %hhu", &rd, &immediate) == 2) {
            binary_instruction = (0x09 << 12) | (rd << 8) | immediate;
        } else if (sscanf(line, "LDR R%hhu %hhu", &rd, &immediate) == 2) {
            binary_instruction = (0x0A << 12) | (rd << 8) | immediate;
        } else {
            printf("Error: Unrecognized instruction \"%s\"\n", line);
            continue;
        }

        // Store the binary instruction in the instruction memory
        cpu->instruction_memory[instruction_index++] = binary_instruction;
    }

    fclose(file);
    printf("Program loaded successfully with %d instructions.\n", instruction_index);
}


void print_cpu_state(CPU *cpu) {
    // TODO: Implement CPU state printing
}

void run_pipeline(CPU *cpu) {

}

void fetch(CPU *cpu) {
    // TODO: Implement fetch stage

}

void decode(CPU *cpu) {
    // TODO: Implement decode stage
}

void execute(CPU *cpu) {
 // TODO: 
}

DecodedInstruction decode_instruction(uint16_t instruction) {
    // TODO: Implement instruction decoding
    DecodedInstruction decoded;
    return decoded;
}

void update_flags(CPU *cpu, uint8_t result) {
    // TODO: Implement flags update
}

void execute_add(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement ADD instruction
}

void execute_sub(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement SUB instruction
}

void execute_mul(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement MUL instruction
}

void execute_movi(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement MOVI instruction
}

void execute_beqz(CPU *cpu, DecodedInstruction instr) {
    if (cpu->registers[instr.rd] == 0) {
        cpu->pc = instr.immediate; // Update PC to branch target
        // Flush the next two instructions
        cpu->IFID.instruction = 0;
        cpu->IDEX.opcode = 0;
        cpu->decode.active = 0;
        cpu->fetch.active = 0;
       /// printf("Instruction %d: BEQZ R%d == 0, Branching to PC = 0x%04X (Flushing Pipeline)\n", cpu->instruction_count, instr.rd, cpu->pc);
    } else {
       // printf("Instruction %d: BEQZ R%d != 0, Continuing execution\n", cpu->instruction_count, instr.rd);
    }
}

void execute_andi(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement ANDI instruction

    cpu->registers[instr.rd] = cpu->registers[instr.rd] & instr.immediate;
    printf("ANDI: R%d = R%d & 0x%02X = 0x%02X\n", instr.rd, instr.rd, instr.immediate, cpu->registers[instr.rd]);
    
}

void execute_eor(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement EOR instruction
    cpu->registers[instr.rd] = cpu->registers[instr.rd] ^ cpu->registers[instr.rs1];
    printf("EOR: R%d = R%d ^ 0x%02X = 0x%02X\n", instr.rd, instr.rd, instr.immediate, cpu->registers[instr.rd]);
    
}

void execute_br(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement BR instruction
        cpu->pc = cpu->registers[instr.rs1];
        printf("BR: PC = R%d -> 0x%04X\n", instr.rs1, cpu->pc);
}

void execute_sal(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement SAL instruction
        cpu->registers[instr.rd] <<= instr.immediate;
        printf("SAL: R%d = R%d << %d -> 0x%02X\n", instr.rd, instr.rd, instr.immediate, cpu->registers[instr.rd]);

}

void execute_sar(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement SAR instruction
        cpu->registers[instr.rd] >>= instr.immediate;
        printf("SAR: R%d = R%d >> %d -> 0x%02X\n", instr.rd, instr.rd, instr.immediate, cpu->registers[instr.rd]);
}

void execute_ldr(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement LDR instruction
    cpu->registers[instr.rd] = cpu->data_memory[instr.immediate];
    printf("LDR: R%d = MEM[R%d + 0x%02X] = 0x%02X\n", instr.rd, instr.rs1, instr.immediate, cpu->registers[instr.rd]);
}

void execute_str(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement STR instruction
    cpu->data_memory[instr.immediate] = cpu->registers[instr.rd];
    printf("STR: MEM[R%d + 0x%02X] = R%d = 0x%02X\n", instr.rs1, instr.immediate, instr.rd, cpu->registers[instr.rd]);
}
