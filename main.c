#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

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

int num_inst = 0;
int reg_used[REGISTER_COUNT] = {0}; // Initialize all elements to 0

// Define structures for the pipeline stages
typedef struct {
    bool active;
} PipelineStage;

typedef struct {
    uint16_t current_Instruction;
    PipelineStage fetch, decode, execute;
    int inst_number;
} Instruction;

typedef struct {
    uint16_t instruction;
    int inst_number;
} IFID;

typedef struct {
    uint8_t opcode;
    uint8_t rd;
    uint8_t rs1;
    uint8_t immediate;
    int inst_number;
    int isempty; // 1 if the stage is empty and 0 if it is full
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

// Function prototypes
void initialize_cpu(CPU *cpu);
void load_program(CPU *cpu, const char *filename);
void print_cpu_state(CPU *cpu);
void run_pipeline(CPU *cpu);
void fetch(CPU *cpu);
void decode(CPU *cpu);
void execute(CPU *cpu);
void End_program(CPU *cpu);
void flush(CPU *cpu, uint8_t imm);

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
        cpu->instruction_memory[i].fetch.active = false;
        cpu->instruction_memory[i].decode.active = false;
        cpu->instruction_memory[i].execute.active = false;
        cpu->instruction_memory[i].inst_number = i + 1;
    }

    for (int i = 0; i < DATA_MEMORY_SIZE; i++) {
        cpu->data_memory[i] = 0;
    }

    for (int i = 0; i < REGISTER_COUNT; i++) {
        cpu->registers[i] = 0;
    }
    // Initial value for a specific register as a trial
    cpu->registers[1] = 1;

    cpu->pc = 0;
    cpu->IFID.instruction = 0;
    cpu->IFID.inst_number = 0;
    cpu->IDEX.immediate = 0;
    cpu->IDEX.opcode = 0;
    cpu->IDEX.rd = 0;
    cpu->IDEX.rs1 = 0;
    cpu->IDEX.inst_number = 0;
    cpu->IDEX.isempty = 1;
    cpu->instruction_count = 0;
    cpu->stall_flag = 0;
}

void End_program(CPU *cpu) {
    // Print out the final values of the PC and SREG
    printf("\nFinal CPU State:\n");
    printf("PC: %d\n", cpu->pc);
    printf("Status Register (SREG): 0x%X\n", cpu->sreg);

    // Print out the values of all non-zero general-purpose registers
    printf("\nRegisters:\n");
    for (int i = 0; i < REGISTER_COUNT; i++) {
         if(reg_used[i]!=0){
            printf("R%d: %d\n", i, cpu->registers[i]);
        }
    }

    // Print out the values of all non-zero locations in the instruction memory
    printf("\nInstruction Memory:\n");
    for (int i = 0; i < INSTRUCTION_MEMORY_SIZE; i++) {
        if (cpu->instruction_memory[i].current_Instruction != 0) {
            printf("Instruction %d: 0x%04X\n", i, cpu->instruction_memory[i].current_Instruction);
        }
    }

    // Print out the values of all non-zero locations in the data memory
    printf("\nData Memory:\n");
    for (int i = 0; i < DATA_MEMORY_SIZE; i++) {
        if (cpu->data_memory[i] != 0) {
            printf("Memory[%d]: %d\n", i, cpu->data_memory[i]);
        }
    }

    printf("\nEnd of Program Execution.\n");
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
        cpu->instruction_memory[instruction_index].inst_number = instruction_index + 1;
        cpu->instruction_count++;
        instruction_index++;
    }

    fclose(file);
    printf("Program loaded successfully with %d instructions.\n", instruction_index);
}

void print_cpu_state(CPU *cpu) {
    printf("Registers:\n");
    for (int i = 0; i < REGISTER_COUNT; i++) {
        if(reg_used[i]!=0){
            printf("R%d: %d\n", i, cpu->registers[i]);
        }
    }
        printf("Data memory:\n");
    for(int i =0 ; i<DATA_MEMORY_SIZE ; i++){
        if(cpu->data_memory[i]!=0){
            printf("Index%d: Value{%d}",i,cpu->data_memory[i]);
        }
    }
}

void run_pipeline(CPU *cpu) {
    int total_instructions = cpu->instruction_count;
    int total_cycles = 3 + (total_instructions - 1);
    int current_cycle = 1;
    while (current_cycle <= total_cycles) {
        printf("Current Cycle is %d and Current PC is %d\n", current_cycle,cpu->pc);

        // Execute, Decode, and Fetch stages in the correct pipeline order
        if (current_cycle >= 3&& current_cycle <= total_instructions+2 ) {
            execute(cpu);
        }
        if (current_cycle >= 2 && current_cycle<= total_instructions+1) {
            decode(cpu);
               printf("IDEX Register inst %d: Opcode=0x%X, RD=%d, RS1=%d, Immediate=0x%X, isempty=%d\n", 
               cpu->IDEX.inst_number, cpu->IDEX.opcode, cpu->IDEX.rd, cpu->IDEX.rs1, cpu->IDEX.immediate, cpu->IDEX.isempty);
        }
        if (current_cycle >= 1 && current_cycle <= total_instructions) {

            fetch(cpu);
            printf("IFID Register inst %d: Instruction=0x%04X  at the PC %d \n", cpu->IFID.inst_number, cpu->IFID.instruction, cpu->pc);


        }

        // Print the IFID and IDEX register values after each cycle
     
        
        print_cpu_state(cpu);
        printf("\n");
            cpu->stall_flag = 0;
        current_cycle++;
    }

    End_program(cpu);
}

void fetch(CPU *cpu) {
    if (cpu->stall_flag) return; // Skip fetch if stalled

    if(cpu->pc < cpu->instruction_count) {
        cpu->IFID.instruction = cpu->instruction_memory[cpu->pc].current_Instruction;
        cpu->IFID.inst_number = cpu->instruction_memory[cpu->pc].inst_number;
        cpu->pc++;
        printf("Fetching Instruction %d:  0x%04X\n", cpu->IFID.inst_number, cpu->IFID.instruction);
    }
}

void decode(CPU *cpu) {
    if (cpu->IFID.instruction != 0 && cpu->IDEX.isempty == 1) {
        uint16_t instruction = cpu->IFID.instruction;
        int inst_num = cpu->IFID.inst_number;

        cpu->IDEX.inst_number = inst_num;
        cpu->IDEX.opcode = (instruction >> 12) & 0xF;  // Extract the opcode (first 4 bits)

        switch (cpu->IDEX.opcode) {
            case 0x00: // ADD
            case 0x01: // SUB
            case 0x02: // MUL
            case 0x06: // EOR
                cpu->IDEX.rd = (instruction >> 8) & 0xF;  // Destination register (next 4 bits)
                cpu->IDEX.rs1 = (instruction >> 4) & 0xF; // Source register 1 (next 4 bits)
                cpu->IDEX.immediate = 0;                  // No immediate value for R-type instructions
                break;

            case 0x03: // MOVI
            case 0x04: // BEQZ
            case 0x05: // ANDI
            case 0x08: // SAL
            case 0x09: // SAR
            case 0x0A: // LDR
            case 0x0B: // STR
                cpu->IDEX.rd = (instruction >> 8) & 0xF;  // Destination register (next 4 bits)
                cpu->IDEX.rs1 = 0;                        // No source register for I-type instructions
                cpu->IDEX.immediate = instruction & 0xFF; // Immediate value (last 8 bits)
                break;

            case 0x07: // BR
                cpu->IDEX.rd = (instruction >> 8) & 0xF;  // Destination register (next 4 bits)
                cpu->IDEX.rs1 = (instruction >> 4) & 0xF; // No source register for BR instruction
                cpu->IDEX.immediate = 0;                  // No immediate value for BR instruction
                cpu->stall_flag = 1;                      // Stall the pipeline for control hazard
                break;

            default:
                printf("Error: Unknown opcode 0x%X\n", cpu->IDEX.opcode);
                cpu->IDEX.isempty = 1;
                return;
        }

        // Mark registers as used
        if (cpu->IDEX.rd != 0) {
            reg_used[cpu->IDEX.rd] = 1;
        }

        if (cpu->IDEX.rs1 != 0) {
            reg_used[cpu->IDEX.rs1] = 1;
        }

        // Update pipeline registers
        cpu->IFID.instruction = 0;  // Clear IFID register after decoding
        cpu->IDEX.isempty = 0;      // Mark the IDEX register as full

        // Print decoded instruction for debugging
        printf("Decoded Instruction %d: Opcode=0x%X, RD=%d, RS1=%d, Immediate=0x%X\n", 
               inst_num, cpu->IDEX.opcode, cpu->IDEX.rd, cpu->IDEX.rs1, cpu->IDEX.immediate);
    }
}

void erase_IDEX(CPU *cpu) {
    // Reset all fields in the IDEX pipeline register
    cpu->IDEX.opcode = 0;
    cpu->IDEX.rd = 0;
    cpu->IDEX.rs1 = 0;
    cpu->IDEX.immediate = 0;
    cpu->IDEX.inst_number = 0;
    cpu->IDEX.isempty = 1;  // Mark the IDEX stage as empty

}
void flush(CPU *cpu, uint8_t imm) {
    printf("Flushing pipeline due to branch instruction with immediate value %d\n", imm);

   
        // Flush both IFID and IDEX registers and branch to PC + Imm instruction if it exists
        printf("Flushing IFID and IDEX registers\n");
        
        // Flush IFID
        printf("Flushing IFID: Instruction=0x%04X, Inst_Num=%d\n", 
               cpu->IFID.instruction, cpu->IFID.inst_number);
        cpu->IFID.instruction = 0;  // Clear IFID register
        cpu->IFID.inst_number = 0;

        // Flush IDEX
        printf("Flushing IDEX: Opcode=0x%X, RD=%d, RS1=%d, Immediate=0x%X, Inst_Num=%d\n", 
               cpu->IDEX.opcode, cpu->IDEX.rd, cpu->IDEX.rs1, cpu->IDEX.immediate, cpu->IDEX.inst_number);
        erase_IDEX(cpu);

        // Update the PC
        cpu->pc += imm -1 ;

        if (cpu->pc >= cpu->instruction_count) {
            printf("Warning: Branch target out of bounds, PC=%d\n", cpu->pc);
        } else {
            printf("Branching to instruction %d at PC=%d\n", cpu->pc + 1, cpu->pc);
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
                    printf("@PC=%d  BEQZ to %d: Branch Taken because R%d = %d\n", cpu->pc, imm, rd, cpu->registers[rd]);
                    flush(cpu, imm);  // Call flush function for branch
                }
                else{
                    printf("BEQZ to %d: Branch Not Taken because R%d = %d\n", imm, rd, cpu->registers[rd]);
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
        if(cpu->stall_flag!=1){
        printf("Executed Instruction %d: Opcode=0x%X, RD=%d, RS1=%d, Immediate=0x%X\n",
               cpu->IDEX.inst_number, cpu->IDEX.opcode, cpu->IDEX.rd, cpu->IDEX.rs1, cpu->IDEX.immediate);
    }}
}
