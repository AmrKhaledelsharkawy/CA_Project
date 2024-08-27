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

typedef struct {
    uint16_t current_Instruction;
    int is_stall; // 1 so the instrucation has already wento into an stage 
} Instruction;

typedef struct {
    uint16_t instruction;
} IFID;

typedef struct {
    int isempty; // 1 if the stage is empty and 0 if it is full
    uint8_t opcode;
    uint8_t rd;
    uint8_t rs1;
    uint8_t immediate;
} IDEX;

typedef struct {
    Instruction instruction_memory[INSTRUCTION_MEMORY_SIZE];
    uint8_t data_memory[DATA_MEMORY_SIZE];
    uint8_t registers[REGISTER_COUNT];
    uint16_t pc;
    PipelineStage fetch, decode, execute;
    uint8_t sreg; // Status Register
    IFID IFID;
    IDEX IDEX;
    int instruction_count;
    int stall_flag; // Flag to indicate control hazard stall
} CPU;

typedef struct {
    uint16_t instruction;
} fetchedInstruction;

typedef struct {
    uint8_t opcode;
    uint8_t rd;
    uint8_t rs1;
    uint8_t immediate;
} DecodedInstruction;

// Function prototypes
void initialize_cpu(CPU *cpu);
void load_program(CPU *cpu, const char *filename);
void print_cpu_state(CPU *cpu);
void run_pipeline(CPU *cpu);
void fetch(CPU *cpu);
void decode(CPU *cpu);
void execute(CPU *cpu);

int main() {
    CPU cpu;
    initialize_cpu(&cpu);
    load_program(&cpu, "program.txt"); // Replace with your program file
    run_pipeline(&cpu);
    return 0;
}

void initialize_cpu(CPU *cpu) {
    for (int i = 0; i < INSTRUCTION_MEMORY_SIZE; i++) {
        cpu->instruction_memory[i].current_Instruction = 0;
        cpu->instruction_memory[i].is_stall = 0;
    }
    
    for (int i = 0; i < DATA_MEMORY_SIZE; i++) {
        cpu->data_memory[i] = 0;
    }
    
    for (int i = 0; i < REGISTER_COUNT; i++) {
        cpu->registers[i] = 0;
    }
    cpu->pc = 0; 
    cpu->IFID.instruction = 0;
    cpu->IDEX.immediate = 0;
    cpu->IDEX.opcode = 0;
    cpu->IDEX.rd = 0;
    cpu->IDEX.rs1 = 0;
    cpu->IDEX.isempty = 1;
    cpu->instruction_count = 0;
    cpu->stall_flag = 0;
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

        uint8_t rd, rs1, immediate;
        uint16_t binary_instruction = 0;

        // Remove newline character if present
        line[strcspn(line, "\n")] = 0;

        // Parse each instruction based on its format
        if (sscanf(line, "MOVI R%hhu, %hhu", &rd, &immediate) == 2) {
            binary_instruction = (0x03 << 12) | (rd << 8) | immediate;
        } else if (sscanf(line, "ADD R%hhu, R%hhu", &rd, &rs1) == 2) {
            binary_instruction = (0x00 << 12) | (rd << 8) | (rs1 << 4);
        } else if (sscanf(line, "SUB R%hhu, R%hhu", &rd, &rs1) == 2) {
            binary_instruction = (0x01 << 12) | (rd << 8) | (rs1 << 4);
        } else if (sscanf(line, "MUL R%hhu, R%hhu", &rd, &rs1) == 2) {
            binary_instruction = (0x02 << 12) | (rd << 8) | (rs1 << 4);
        } else if (sscanf(line, "BEQZ R%hhu, %hhu", &rd, &immediate) == 2) {
            binary_instruction = (0x04 << 12) | (rd << 8) | immediate;
        } else if (sscanf(line, "ANDI R%hhu, %hhu", &rd, &immediate) == 2) {
            binary_instruction = (0x05 << 12) | (rd << 8) | immediate;
        } else if (sscanf(line, "EOR R%hhu, R%hhu", &rd, &rs1) == 2) {
            binary_instruction = (0x06 << 12) | (rd << 8) | (rs1 << 4);
        } else if (sscanf(line, "BR R%hhu, R%hhu", &rd, &rs1) == 2) {
            binary_instruction = (0x07 << 12) | (rd << 8) | (rs1 << 4);
        } else if (sscanf(line, "SAL R%hhu, %hhu", &rd, &immediate) == 2) {
            binary_instruction = (0x08 << 12) | (rd << 8) | immediate;
        } else if (sscanf(line, "SAR R%hhu, %hhu", &rd, &immediate) == 2) {
            binary_instruction = (0x09 << 12) | (rd << 8) | immediate;
        } else if (sscanf(line, "LDR R%hhu, %hhu", &rd, &immediate) == 2) {
            binary_instruction = (0x0A << 12) | (rd << 8) | immediate;
        } else if (sscanf(line, "STR R%hhu, %hhu", &rd, &immediate) == 2) {
            binary_instruction = (0x0B << 12) | (rd << 8) | immediate;
        } else {
            printf("Error: Unrecognized instruction \"%s\"\n", line);
            continue;
        }

        // Store the binary instruction in the instruction memory
        cpu->instruction_memory[instruction_index].current_Instruction = binary_instruction;
        cpu->instruction_memory[instruction_index].is_stall = 0;
        cpu->instruction_count++;
        instruction_index++;
    }

    fclose(file);
    printf("Program loaded successfully with %d instructions.\n", instruction_index);
}

void print_cpu_state(CPU *cpu) {
    printf("PC: %d\n", cpu->pc);
    printf("Registers:\n");
    for (int i = 0; i < REGISTER_COUNT; i++) {
        printf("R%d: %d\n", i, cpu->registers[i]);
    }
    printf("Status Register: 0x%X\n", cpu->sreg);
}

void run_pipeline(CPU *cpu) {
    int total_Cycles = 3 + (cpu->instruction_count - 1); // 7int total = 9
    int Current_cycle = 1;
    while (Current_cycle <= total_Cycles) {
        execute(cpu);
        decode(cpu);
        fetch(cpu);

        for(int i = 0; i< INSTRUCTION_MEMORY_SIZE; i++){
            cpu->instruction_memory[i].is_stall = 0; 
        }
        Current_cycle++;
    }
}

void fetch(CPU *cpu) {
    if (cpu->stall_flag) return; // Skip fetch if stalled

    if(cpu->IFID.instruction == 0 && cpu->instruction_memory[cpu->pc].is_stall == 0) {
        cpu->IFID.instruction = cpu->instruction_memory[cpu->pc].current_Instruction;
        cpu->instruction_memory[cpu->pc].is_stall = 1;
        printf("Instruction %d: Fetching 0x%04X\n", cpu->instruction_count, cpu->IFID.instruction);
        cpu->pc++;
    }
}

void decode(CPU *cpu) {
    if(cpu->IFID.instruction != 0 && cpu->IDEX.isempty == 1 && cpu->stall_flag == 0) {
        uint16_t instruction = cpu->IFID.instruction;
        cpu->IFID.instruction = 0;

        cpu->IDEX.opcode = (instruction >> 12) & 0xF;
        
        switch(cpu->IDEX.opcode) {
            case 0x00: // ADD
            case 0x01: // SUB
            case 0x02: // MUL
            case 0x06: // EOR
                cpu->IDEX.rd = (instruction >> 8) & 0xF;
                cpu->IDEX.rs1 = (instruction >> 4) & 0xF;
                cpu->IDEX.immediate = 0; // Not used for R-type instructions
                break;

            case 0x03: // MOVI
            case 0x04: // BEQZ
            case 0x05: // ANDI
            case 0x08: // SAL
            case 0x09: // SAR
            case 0x0A: // LDR
                cpu->IDEX.rd = (instruction >> 8) & 0xF;
                cpu->IDEX.rs1 = 0; // Not used for I-type instructions
                cpu->IDEX.immediate = instruction & 0xFF;
                break;

            case 0x07: // BR
                cpu->IDEX.rd = (instruction >> 8) & 0xF;
                cpu->IDEX.rs1 = 0;
                cpu->IDEX.immediate = 0; // Not used for BR instruction
                cpu->stall_flag = 1; // Stall the pipeline for control hazard
                break;

            default:
                printf("Error: Unknown opcode 0x%X\n", cpu->IDEX.opcode);
                break;
        }

        cpu->IDEX.isempty = 0;

        printf("Decoded Instruction: Opcode=0x%X, RD=%d, RS1=%d, Immediate=0x%X\n",
               cpu->IDEX.opcode, cpu->IDEX.rd, cpu->IDEX.rs1, cpu->IDEX.immediate);
    }
}

void execute(CPU *cpu) {
    if(cpu->IDEX.isempty == 0 && cpu->stall_flag == 0) {
        uint8_t opcode = cpu->IDEX.opcode;
        uint8_t rd = cpu->IDEX.rd;
        uint8_t rs = cpu->IDEX.rs1;
        uint8_t imm = cpu->IDEX.immediate;

        switch (opcode) {
            case 0x00: // ADD
                cpu->registers[rd] += cpu->registers[rs];
                break;
            case 0x01: // SUB
                cpu->registers[rd] -= cpu->registers[rs];
                break;
            case 0x02: // MUL
                cpu->registers[rd] *= cpu->registers[rs];
                break;
            case 0x03: // MOVI
                cpu->registers[rd] = imm;
                break;
            case 0x04: // BEQZ
                if (cpu->registers[rd] == 0) {
                    cpu->pc += imm;
                    cpu->stall_flag = 0; // Clear stall after branch
                }
                break;
            case 0x05: // ANDI
                cpu->registers[rd] &= imm;
                break;
            case 0x06: // EOR
                cpu->registers[rd] ^= cpu->registers[rs];
                break;
            case 0x07: // BR
                cpu->pc = cpu->registers[rd] | cpu->registers[rs];
                cpu->stall_flag = 0; // Clear stall after branch
                break;
            case 0x08: // SAL
                cpu->registers[rd] <<= imm;
                break;
            case 0x09: // SAR
                cpu->registers[rd] >>= imm;
                break;
            case 0x0A: // LDR
                cpu->registers[rd] = cpu->data_memory[imm];
                break;
            case 0x0B: // STR
                cpu->data_memory[imm] = cpu->registers[rd];
                break;
            default:
                printf("Error: Unknown opcode 0x%X\n", opcode);
                break;
        }

        cpu->IDEX.isempty = 1;
        printf("Executed Instruction: Opcode=0x%X, RD=%d, RS1=%d, Immediate=0x%X\n",
               cpu->IDEX.opcode, cpu->IDEX.rd, cpu->IDEX.rs1, cpu->IDEX.immediate);
    }
}