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
    int8_t immediate;  // Use int8_t to handle negative values for certain instructions
    int inst_number;
    int isempty; // 1 if the stage is empty and 0 if it is full
} IDEX;

typedef struct {
    Instruction instruction_memory[INSTRUCTION_MEMORY_SIZE];
    uint8_t data_memory[DATA_MEMORY_SIZE];
    int8_t registers[REGISTER_COUNT];  // Change to int8_t for signed values
    PipelineStage fetch, decode, execute;
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
void flush_BR(CPU *cpu, uint16_t new_pc);
void erase_IDEX(CPU *cpu);
void update_status_register(CPU *cpu, int8_t result, uint8_t rd, uint8_t rs);

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

    // Set PC and SREG to their initial locations in the register file
    cpu->registers[64] = 0; // PC is R64
    cpu->registers[65] = 0; // SREG is R65

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
    printf("PC: %d\n", cpu->registers[64]);

    // Print the SREG in a table format
    printf("Status Register (SREG):\n");
    printf("7 | 6 | 5 | C | V | N | S | Z\n");
    printf("-------------------------------\n");
    for (int i = 7; i >= 0; i--) {
        if (i == 7 || i == 6 || i == 5) {
            printf(" X |"); // Reserved bits (Bits 7, 6, 5) are always 0
        } else {
            printf(" %d |", (cpu->registers[65] >> i) & 0x01);
        }
    }
    printf("\n");

    // Print out the values of all non-zero general-purpose registers
    printf("\nRegisters:\n");
    for (int i = 0; i < 64; i++) {
        if (   cpu->registers[i] != 0 && i != 64 && i != 65) {
            printf("R%d: %d\n", i, cpu->registers[i]);  // Print as signed integer
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

        uint8_t rd, rs1;
        int16_t immediate;  // Use int16_t to capture potential negative values in load
        uint16_t binary_instruction = 0;

        // Remove newline character if present
        line[strcspn(line, "\n")] = 0;

        // Parse each instruction based on its format
        if (sscanf(line, "MOVI R%hhu, %hd", &rd, &immediate) == 2) {
            binary_instruction = (0x03 << 12) | (rd << 8) | (immediate & 0x3F);
        } else if (sscanf(line, "ADD R%hhu, R%hhu", &rd, &rs1) == 2) {
            binary_instruction = (0x00 << 12) | (rd << 8) | (rs1 << 4);
        } else if (sscanf(line, "SUB R%hhu, R%hhu", &rd, &rs1) == 2) {
            binary_instruction = (0x01 << 12) | (rd << 8) | (rs1 << 4);
        } else if (sscanf(line, "MUL R%hhu, R%hhu", &rd, &rs1) == 2) {
            binary_instruction = (0x02 << 12) | (rd << 8) | (rs1 << 4);
        } else if (sscanf(line, "BEQZ R%hhu, %hd", &rd, &immediate) == 2) {
            binary_instruction = (0x04 << 12) | (rd << 8) | (immediate & 0x3F);
        } else if (sscanf(line, "ANDI R%hhu, %hd", &rd, &immediate) == 2) {
            if (immediate < 0 || immediate > 63) {
                printf("Error: ANDI instruction with invalid immediate value %d (valid range is 0-63)\n", immediate);
                continue;
            }
            binary_instruction = (0x05 << 12) | (rd << 8) | (immediate & 0x3F);
        } else if (sscanf(line, "EOR R%hhu, R%hhu", &rd, &rs1) == 2) {
            binary_instruction = (0x06 << 12) | (rd << 8) | (rs1 << 4);
        } else if (sscanf(line, "BR R%hhu, R%hhu", &rd, &rs1) == 2) {
            binary_instruction = (0x07 << 12) | (rd << 8) | (rs1 << 4);
        } else if (sscanf(line, "SAL R%hhu, %hd", &rd, &immediate) == 2) {
            if (immediate < 0 || immediate > 63) {
                printf("Error: SAL instruction with invalid immediate value %d (valid range is 0-63)\n", immediate);
                continue;
            }
            binary_instruction = (0x08 << 12) | (rd << 8) | (immediate & 0x3F);
        } else if (sscanf(line, "SAR R%hhu, %hd", &rd, &immediate) == 2) {
            if (immediate < 0 || immediate > 63) {
                printf("Error: SAR instruction with invalid immediate value %d (valid range is 0-63)\n", immediate);
                continue;
            }
            binary_instruction = (0x09 << 12) | (rd << 8) | (immediate & 0x3F);
        } else if (sscanf(line, "LDR R%hhu, %hd", &rd, &immediate) == 2) {
            if (immediate < 0 || immediate > 63) {
                printf("Error: LDR instruction with invalid immediate value %d (valid range is 0-63)\n", immediate);
                continue;
            }
            binary_instruction = (0x0A << 12) | (rd << 8) | (immediate & 0x3F);
        } else if (sscanf(line, "STR R%hhu, %hd", &rd, &immediate) == 2) {
            if (immediate < 0 || immediate > 63) {
                printf("Error: STR instruction with invalid immediate value %d (valid range is 0-63)\n", immediate);
                continue;
            }
            binary_instruction = (0x0B << 12) | (rd << 8) | (immediate & 0x3F);
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
    for (int i = 0; i < 64; i++) {
        if (reg_used[i] != 0) {
            printf("R%d: %d\n", i, cpu->registers[i]);  // Print as signed integer
        }
    }
    printf("PC: %d\n", cpu->registers[64]);
    printf("SREG: 0x%X\n", cpu->registers[65]);
    printf("Data memory:\n");
    for (int i = 0; i < DATA_MEMORY_SIZE; i++) {
        if (cpu->data_memory[i] != 0) {
            printf("Index %d: Value {%d}\n", i, cpu->data_memory[i]);
        }
    }
}

void run_pipeline(CPU *cpu) {
    int total_instructions = cpu->instruction_count;
    int total_cycles = 3 + (total_instructions - 1);
    int current_cycle = 1;
    while (current_cycle <= total_cycles) {
        printf("Current Cycle is %d and Current PC is %d\n", current_cycle, cpu->registers[64]);

        // Execute, Decode, and Fetch stages in the correct pipeline order
        if (current_cycle >= 3 && current_cycle <= total_instructions + 2) {
            execute(cpu);
        }
        if (current_cycle >= 2 && current_cycle <= total_instructions + 1) {
            decode(cpu);
            printf("IDEX Register inst %d: Opcode=0x%X, RD=%d, RS1=%d, Immediate=0x%X, isempty=%d\n",
                   cpu->IDEX.inst_number, cpu->IDEX.opcode, cpu->IDEX.rd, cpu->IDEX.rs1, cpu->IDEX.immediate, cpu->IDEX.isempty);
        }
        if (current_cycle >= 1 && current_cycle <= total_instructions) {
            fetch(cpu);
            printf("IFID Register inst %d: Instruction=0x%04X at the PC %d \n", cpu->IFID.inst_number, cpu->IFID.instruction, cpu->registers[64]);
        }

        print_cpu_state(cpu);
        printf("\n");
        cpu->stall_flag = 0;
        current_cycle++;
    }

    End_program(cpu);
}

void fetch(CPU *cpu) {
    if (cpu->stall_flag) return; // Skip fetch if stalled

    if (cpu->registers[64] < cpu->instruction_count) {
        cpu->IFID.instruction = cpu->instruction_memory[cpu->registers[64]].current_Instruction;
        cpu->IFID.inst_number = cpu->instruction_memory[cpu->registers[64]].inst_number;
        cpu->registers[64]++;
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
                cpu->IDEX.rd = (instruction >> 8) & 0xF;  // Destination register (next 4 bits)
                cpu->IDEX.rs1 = 0;                        // No source register for I-type instructions
                cpu->IDEX.immediate = instruction & 0x3F; // Immediate value (last 6 bits)
                if (cpu->IDEX.immediate & 0x20) { // Handle sign extension for negative IMM
                    cpu->IDEX.immediate |= 0xC0;
                }
                break;

            case 0x05: // ANDI
            case 0x08: // SAL
            case 0x09: // SAR
            case 0x0A: // LDR
            case 0x0B: // STR
                cpu->IDEX.rd = (instruction >> 8) & 0xF;  // Destination register (next 4 bits)
                cpu->IDEX.rs1 = 0;                        // No source register for I-type instructions
                cpu->IDEX.immediate = instruction & 0x3F; // Immediate value (last 6 bits)
                if (cpu->IDEX.immediate < 0 || cpu->IDEX.immediate > 63) {
                    printf("Error: Instruction with invalid immediate value %d (valid range is 0-63)\n", cpu->IDEX.immediate);
                    erase_IDEX(cpu);
                    return;
                }
                break;

            case 0x07: // BR
                cpu->IDEX.rd = (instruction >> 8) & 0xF;  // Destination register (next 4 bits)
                cpu->IDEX.rs1 = (instruction >> 4) & 0xF; // Source register 1 (next 4 bits)
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
    uint16_t new_pc = cpu->registers[64] + (imm - 1);
    if (new_pc >= 0) {
        cpu->registers[64] = new_pc;
    } else {
        printf("Branch is cancelled due to negative PC\n");
        return;
    }

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

    if (cpu->registers[64] >= cpu->instruction_count) {
        printf("Warning: Branch target out of bounds, PC=%d\n", cpu->registers[64]);
    } else {
        printf("Branching to instruction %d at PC=%d\n", cpu->registers[64] + 1, cpu->registers[64]);
    }
}

void flush_BR(CPU *cpu, uint16_t new_pc) {
    if (new_pc < 0) {
        printf("Branch is cancelled due to negative PC\n");
        return;
    }
    printf("Flushing pipeline due to BR instruction with new PC value %d\n", new_pc);

    // Flush both IFID and IDEX registers and branch to new PC
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

    cpu->registers[64] = new_pc - 1;

    if (cpu->registers[64] >= cpu->instruction_count) {
        printf("Warning: Branch target out of bounds, PC=%d\n", cpu->registers[64]);
    } else {
        printf("Branching to instruction %d at PC=%d\n", cpu->registers[64] + 1, cpu->registers[64]);
    }
}

void update_status_register(CPU *cpu, int8_t result, uint8_t rd, uint8_t rs) {
    uint8_t sreg = 0;

    // Carry Flag (C)
    if (((int16_t)cpu->registers[rd] + (int16_t)cpu->registers[rs]) > 127 ||
        ((int16_t)cpu->registers[rd] - (int16_t)cpu->registers[rs]) < -128) {
        sreg |= CARRY_FLAG;
    }

    // Two's Complement Overflow Flag (V)
    int8_t signed_rd = cpu->registers[rd];
    int8_t signed_rs = cpu->registers[rs];
    if (((signed_rd > 0) && (signed_rs > 0) && (result < 0)) || 
        ((signed_rd < 0) && (signed_rs < 0) && (result > 0))) {
        sreg |= OVERFLOW_FLAG;
    }

    // Negative Flag (N)
    if (result < 0) {
        sreg |= NEGATIVE_FLAG;
    }

    // Zero Flag (Z)
    if (result == 0) {
        sreg |= ZERO_FLAG;
    }

    // Sign Flag (S) = N ⊕ V
    if (((sreg & NEGATIVE_FLAG) >> 2) ^ ((sreg & OVERFLOW_FLAG) >> 1)) {
        sreg |= SIGN_FLAG;
    }

    cpu->registers[65] = sreg; // Update SREG
}

void execute(CPU *cpu) {
    if (cpu->IDEX.isempty == 0 && cpu->stall_flag == 0) {
        uint8_t opcode = cpu->IDEX.opcode;
        uint8_t rd = cpu->IDEX.rd;
        uint8_t rs = cpu->IDEX.rs1;
        int8_t imm = cpu->IDEX.immediate;
        int8_t result = 0;

        switch (opcode) {
            case 0x00: // ADD
                result = cpu->registers[rd] + cpu->registers[rs];
                cpu->registers[rd] = result;
                update_status_register(cpu, result, rd, rs);
                break;
            case 0x01: // SUB
                result = cpu->registers[rd] - cpu->registers[rs];
                cpu->registers[rd] = result;
                update_status_register(cpu, result, rd, rs);
                break;
            case 0x02: // MUL
                result = cpu->registers[rd] * cpu->registers[rs];
                cpu->registers[rd] = result;
                update_status_register(cpu, result, rd, rs);
                break;
            case 0x03: // MOVI
                cpu->registers[rd] = imm;  // Store signed value directly
                update_status_register(cpu, imm, rd, 0);
                break;
            case 0x04: // BEQZ
                if (cpu->registers[rd] == 0) {
                    printf("@PC=%d  BEQZ to %d: Branch Taken because R%d = %d\n", cpu->registers[64], imm, rd, cpu->registers[rd]);
                    flush(cpu, imm);  // Call flush function for branch
                } else {
                    printf("BEQZ to %d: Branch Not Taken because R%d = %d\n", imm, rd, cpu->registers[rd]);
                }
                break;
            case 0x05: // ANDI
                if (imm >= 0 && imm <= 63) {
                    result = cpu->registers[rd] & imm;
                    cpu->registers[rd] = result;
                    update_status_register(cpu, result, rd, 0);
                } else {
                    printf("Error: ANDI executed with invalid immediate value %d (valid range is 0-63)\n", imm);
                }
                break;
            case 0x06: // EOR
                result = cpu->registers[rd] ^ cpu->registers[rs];
                cpu->registers[rd] = result;
                update_status_register(cpu, result, rd, rs);
                break;
            case 0x07: { // BR
                // Concatenate R1 and R2 and take the first 10 bits
                uint16_t concat_value = ((uint16_t)cpu->registers[rd] << 8) | cpu->registers[rs];
                uint16_t new_pc = concat_value >> 6;
                flush_BR(cpu, new_pc);
                break;
            }
            case 0x08: // SAL
                if (imm >= 0 && imm <= 63) {
                    result = cpu->registers[rd] << imm;
                    cpu->registers[rd] = result;
                    update_status_register(cpu, result, rd, 0);
                } else {
                    printf("Error: SAL executed with invalid immediate value %d (valid range is 0-63)\n", imm);
                }
                break;
            case 0x09: // SAR
                if (imm >= 0 && imm <= 63) {
                    result = cpu->registers[rd] >> imm;
                    cpu->registers[rd] = result;
                    update_status_register(cpu, result, rd, 0);
                } else {
                    printf("Error: SAR executed with invalid immediate value %d (valid range is 0-63)\n", imm);
                }
                break;
            case 0x0A: // LDR
                if (imm >= 0 && imm <= 63) {
                    result = cpu->data_memory[imm];
                    cpu->registers[rd] = result;
                    update_status_register(cpu, result, rd, 0);
                } else {
                    printf("Error: LDR executed with invalid immediate value %d (valid range is 0-63)\n", imm);
                }
                break;
            case 0x0B: // STR
                if (imm >= 0 && imm <= 63) {
                    cpu->data_memory[imm] = cpu->registers[rd];
                } else {
                    printf("Error: STR executed with invalid immediate value %d (valid range is 0-63)\n", imm);
                }
                break;
            default:
                printf("Error: Unknown opcode 0x%X\n", opcode);
                break;
        }

        cpu->IDEX.isempty = 1;
        if (cpu->stall_flag != 1) {
            printf("Executed Instruction %d: Opcode=0x%X, RD=%d, RS1=%d, Immediate=0x%X\n",
                   cpu->IDEX.inst_number, cpu->IDEX.opcode, cpu->IDEX.rd, cpu->IDEX.rs1, cpu->IDEX.immediate);
        }
    }
}
