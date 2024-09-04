#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// Define constants
#define INSTRUCTION_MEMORY_SIZE 1024 // 16-bit words
#define DATA_MEMORY_SIZE 2048        // 8-bit bytes
#define REGISTER_COUNT 66

// Define file paths using #define for output and error log files
#define OUTPUT_FILE_PATH "/home/amrotheone/ui/CA_Project/cycledata.txt"
#define ERROR_LOG_FILE_PATH "errorlog.txt"

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
void print_cpu_state(CPU *cpu, FILE *output_file);
void run_pipeline(CPU *cpu);
void fetch(CPU *cpu, FILE *error_file);
void decode(CPU *cpu, FILE *error_file);
void execute(CPU *cpu, FILE *error_file);
void End_program(CPU *cpu);
void flush(CPU *cpu, uint8_t imm, FILE *error_file);
void flush_BR(CPU *cpu, uint16_t new_pc, FILE *error_file);
void erase_IDEX(CPU *cpu);
void update_status_register(CPU *cpu, int8_t result, uint8_t rd, uint8_t rs);
void write_cycle_data(CPU *cpu, int cycle, FILE *output_file);

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
    FILE *output_file = fopen(OUTPUT_FILE_PATH, "a");
    FILE *error_file = fopen(ERROR_LOG_FILE_PATH, "a");

    if (!output_file || !error_file) {
        printf("Error: Unable to open output or error file.\n");
        return;
    }

    // Print out the final values of the PC and SREG
    fprintf(output_file, "\nFinal CPU State:\n");
    fprintf(output_file, "PC: %d\n", cpu->registers[64]);

    // Print the SREG in a table format
    fprintf(output_file, "Status Register (SREG):\n");
    fprintf(output_file, "7 | 6 | 5 | C | V | N | S | Z\n");
    fprintf(output_file, "-------------------------------\n");
    for (int i = 7; i >= 0; i--) {
        if (i == 7 || i == 6 || i == 5) {
            fprintf(output_file, " X |"); // Reserved bits (Bits 7, 6, 5) are always 0
        } else {
            fprintf(output_file, " %d |", (cpu->registers[65] >> i) & 0x01);
        }
    }
    fprintf(output_file, "\n");

    // Print out the values of all non-zero general-purpose registers
    fprintf(output_file, "\nRegisters:\n");
    for (int i = 0; i < 64; i++) {
        if (i!=64 && i!=65) {
            fprintf(output_file, "R%d: %d\n", i, cpu->registers[i]);  // Print as signed integer
        }
    }

    // Print out the values of all non-zero locations in the instruction memory
    fprintf(output_file, "\nInstruction Memory:\n");
    for (int i = 0; i < INSTRUCTION_MEMORY_SIZE; i++) {
        if (cpu->instruction_memory[i].current_Instruction != 0) {
            fprintf(output_file, "Instruction %d: 0x%04X\n", i, cpu->instruction_memory[i].current_Instruction);
        }
    }

    // Print out the values of all non-zero locations in the data memory
    fprintf(output_file, "\nData Memory:\n");
    for (int i = 0; i < DATA_MEMORY_SIZE; i++) {
        if (cpu->data_memory[i] != 0) {
            fprintf(output_file, "Memory[%d]: %d\n", i, cpu->data_memory[i]);
        }
    }

    fprintf(output_file, "\nEnd of Program Execution.\n");
    fclose(output_file);
    fclose(error_file);
}

void load_program(CPU *cpu, const char *filename) {
    FILE *file = fopen(filename, "r");
    FILE *error_file = fopen(ERROR_LOG_FILE_PATH, "a");

 // Check if files are opened successfully
    if (!file) {
        if (error_file) {
            fprintf(error_file, "Error: Unable to open file %s\n", filename);
            fclose(error_file);
        }
        printf("Error: Unable to open file %s\n", filename);
        return;
    }

    if (!error_file) {
        printf("Error: Unable to open error log file %s\n", ERROR_LOG_FILE_PATH);
        fclose(file);
        return;
    }

    char line[256];
    int instruction_index = 0;

    // Initialize all instruction memory to prevent uninitialized access
    memset(cpu->instruction_memory, 0, sizeof(cpu->instruction_memory));

    while (fgets(line, sizeof(line), file)) {
        if (instruction_index >= INSTRUCTION_MEMORY_SIZE) {
            fprintf(error_file, "Error: Program too large to fit in instruction memory.\n");
            break;
        }

        

        uint8_t rd = 0, rs1 = 0;
        int16_t immediate = 0;  // Use int16_t to capture potential negative values in load
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
                fprintf(error_file, "Error: ANDI instruction with invalid immediate value %d (valid range is 0-63)\n", immediate);
                continue;
            }
            binary_instruction = (0x05 << 12) | (rd << 8) | (immediate & 0x3F);
        } else if (sscanf(line, "EOR R%hhu, R%hhu", &rd, &rs1) == 2) {
            binary_instruction = (0x06 << 12) | (rd << 8) | (rs1 << 4);
        } else if (sscanf(line, "BR R%hhu, R%hhu", &rd, &rs1) == 2) {
            binary_instruction = (0x07 << 12) | (rd << 8) | (rs1 << 4);
        } else if (sscanf(line, "SAL R%hhu, %hd", &rd, &immediate) == 2) {
            if (immediate < 0 || immediate > 63) {
                fprintf(error_file, "Error: SAL instruction with invalid immediate value %d (valid range is 0-63)\n", immediate);
                continue;
            }
            binary_instruction = (0x08 << 12) | (rd << 8) | (immediate & 0x3F);
        } else if (sscanf(line, "SAR R%hhu, %hd", &rd, &immediate) == 2) {
            if (immediate < 0 || immediate > 63) {
                fprintf(error_file, "Error: SAR instruction with invalid immediate value %d (valid range is 0-63)\n", immediate);
                continue;
            }
            binary_instruction = (0x09 << 12) | (rd << 8) | (immediate & 0x3F);
        } else if (sscanf(line, "LDR R%hhu, %hd", &rd, &immediate) == 2) {
            if (immediate < 0 || immediate > 63) {
                fprintf(error_file, "Error: LDR instruction with invalid immediate value %d (valid range is 0-63)\n", immediate);
                continue;
            }
            binary_instruction = (0x0A << 12) | (rd << 8) | (immediate & 0x3F);
        } else if (sscanf(line, "STR R%hhu, %hd", &rd, &immediate) == 2) {
            if (immediate < 0 || immediate > 63) {
                fprintf(error_file, "Error: STR instruction with invalid immediate value %d (valid range is 0-63)\n", immediate);
                continue;
            }
            binary_instruction = (0x0B << 12) | (rd << 8) | (immediate & 0x3F);
        } else {
            fprintf(error_file, "Error: Unrecognized instruction \"%s\"\n", line);
            continue;
        }

        // Ensure the instruction_index is within bounds
        if (instruction_index < 0 || instruction_index >= INSTRUCTION_MEMORY_SIZE) {
            fprintf(error_file, "Error: Instruction index out of bounds: %d\n", instruction_index);
            break;
        }

        // Store the binary instruction in the instruction memory
        cpu->instruction_memory[instruction_index].current_Instruction = binary_instruction;
        cpu->instruction_memory[instruction_index].inst_number = instruction_index + 1;
        cpu->instruction_count++;
        instruction_index++;
    }

    fclose(file);
    fclose(error_file);
    printf("Program loaded successfully with %d instructions.\n", instruction_index);
}

void print_cpu_state(CPU *cpu, FILE *output_file) {
    fprintf(output_file, "Registers:\n");
    for (int i = 0; i < 64; i++) {
        if (reg_used[i] != 0) {
            fprintf(output_file, "R%d: %d\n", i, cpu->registers[i]);  // Print as signed integer
        }
    }
    fprintf(output_file, "PC: %d\n", cpu->registers[64]);
    fprintf(output_file, "SREG: 0x%X\n", cpu->registers[65]);
    fprintf(output_file, "Data memory:\n");
    for (int i = 0; i < DATA_MEMORY_SIZE; i++) {
        if (cpu->data_memory[i] != 0) {
            fprintf(output_file, "Index %d: Value {%d}\n", i, cpu->data_memory[i]);
        }
    }
}

void run_pipeline(CPU *cpu) {
    int total_instructions = cpu->instruction_count;
    int total_cycles = 3 + (total_instructions - 1);
    int current_cycle = 1;

    FILE *output_file = fopen(OUTPUT_FILE_PATH, "w"); // Open the output file for writing
    FILE *error_file = fopen(ERROR_LOG_FILE_PATH, "w");   // Open the error log file for writing

    if (!output_file || !error_file) {
        printf("Error: Unable to open output or error file.\n");
        return;
    }

    // Start of cycle data identifier
    while (current_cycle <= total_cycles) {
        fprintf(output_file, "### START OF CYCLE%d DATA ###\n", current_cycle);

        fprintf(output_file, "Cycle %d:\n", current_cycle);
        fprintf(output_file, "Current Cycle is %d and Current PC is %d\n", current_cycle, cpu->registers[64]);

        // Execute stage
        if (current_cycle >= 3 && current_cycle <= total_instructions + 2) {
            fprintf(output_file, "Executing Instruction %d: Opcode=0x%X, RD=%d, RS1=%d, Immediate=0x%X\n",
                    cpu->IDEX.inst_number, cpu->IDEX.opcode, cpu->IDEX.rd, cpu->IDEX.rs1, cpu->IDEX.immediate);
            execute(cpu, error_file);
        }

        // Decode stage
        if (current_cycle >= 2 && current_cycle <= total_instructions + 1) {
            fprintf(output_file, "Decoding Instruction %d: Opcode=0x%X, RD=%d, RS1=%d, Immediate=0x%X\n",
                    cpu->IFID.inst_number, (cpu->IFID.instruction >> 12) & 0xF, (cpu->IFID.instruction >> 8) & 0xF,
                    (cpu->IFID.instruction >> 4) & 0xF, cpu->IFID.instruction & 0x3F);
            decode(cpu, error_file);
            fprintf(output_file, "IDEX Register inst %d: Opcode=0x%X, RD=%d, RS1=%d, Immediate=0x%X, isempty=%d\n",
                    cpu->IDEX.inst_number, cpu->IDEX.opcode, cpu->IDEX.rd, cpu->IDEX.rs1, cpu->IDEX.immediate, cpu->IDEX.isempty);
        }

        // Fetch stage
        if (current_cycle >= 1 && current_cycle <= total_instructions) {
        
            fetch(cpu, error_file);
            fprintf(output_file, "Fetching Instruction %d: Instruction=0x%04X at the PC %d \n",
                    cpu->registers[64], cpu->IFID.instruction, cpu->registers[64]);
        }

        cpu->stall_flag = 0;

        // Print registers for the current cycle
        fprintf(output_file, "### Registers CYCLE%d DATA ###\n", current_cycle);
        fprintf(output_file, "Registers:\n");
        for (int i = 0; i < 64; i++) {
            if (cpu->registers[i] != 0) {
                fprintf(output_file, "R%d: %d\n", i, cpu->registers[i]);
            }
        }
        fprintf(output_file, "PC: %d\n", cpu->registers[64]);
        fprintf(output_file, "SREG: 0x%X\n", cpu->registers[65]);

        // Print data memory for the current cycle
        fprintf(output_file, "### Data Memory CYCLE%d DATA ###\n", current_cycle);
        fprintf(output_file, "Data Memory:\n");
        for (int i = 0; i < DATA_MEMORY_SIZE; i++) {
            if (cpu->data_memory[i] != 0) {
                fprintf(output_file, "Memory[%d]: %d\n", i, cpu->data_memory[i]);
            }
        }

        // End of cycle data identifier
        fprintf(output_file, "### END OF CYCLE%d DATA ###\n", current_cycle);
        current_cycle++;
    }

    fclose(output_file); // Close the output file
    fclose(error_file);  // Close the error file
    End_program(cpu);
}

void fetch(CPU *cpu, FILE *error_file) {
    if (cpu->stall_flag) return; // Skip fetch if stalled

    if (cpu->registers[64] < cpu->instruction_count) {
        cpu->IFID.instruction = cpu->instruction_memory[cpu->registers[64]].current_Instruction;
        cpu->IFID.inst_number = cpu->instruction_memory[cpu->registers[64]].inst_number;
        cpu->registers[64]++;
        fprintf(error_file, "Fetching Instruction %d:  0x%04X\n", cpu->IFID.inst_number, cpu->IFID.instruction);
    }
}

void decode(CPU *cpu, FILE *error_file) {
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
                    fprintf(error_file, "Error: Instruction with invalid immediate value %d (valid range is 0-63)\n", cpu->IDEX.immediate);
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
                fprintf(error_file, "Error: Unknown opcode 0x%X\n", cpu->IDEX.opcode);
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
        fprintf(error_file, "Decoded Instruction %d: Opcode=0x%X, RD=%d, RS1=%d, Immediate=0x%X\n", 
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

void flush(CPU *cpu, uint8_t imm, FILE *error_file) {
    fprintf(error_file, "Flushing pipeline due to branch instruction with immediate value %d\n", imm);
    uint16_t new_pc = cpu->registers[64] + (imm - 1);
    if (new_pc >= 0) {
        cpu->registers[64] = new_pc;
    } else {
        fprintf(error_file, "Branch is cancelled due to negative PC\n");
        return;
    }

    // Flush both IFID and IDEX registers and branch to PC + Imm instruction if it exists
    fprintf(error_file, "Flushing IFID and IDEX registers\n");

    // Flush IFID
    fprintf(error_file, "Flushing IFID: Instruction=0x%04X, Inst_Num=%d\n",
           cpu->IFID.instruction, cpu->IFID.inst_number);
    cpu->IFID.instruction = 0;  // Clear IFID register
    cpu->IFID.inst_number = 0;

    // Flush IDEX
    fprintf(error_file, "Flushing IDEX: Opcode=0x%X, RD=%d, RS1=%d, Immediate=0x%X, Inst_Num=%d\n",
           cpu->IDEX.opcode, cpu->IDEX.rd, cpu->IDEX.rs1, cpu->IDEX.immediate, cpu->IDEX.inst_number);
    erase_IDEX(cpu);

    if (cpu->registers[64] >= cpu->instruction_count) {
        fprintf(error_file, "Warning: Branch target out of bounds, PC=%d\n", cpu->registers[64]);
    } else {
        fprintf(error_file, "Branching to instruction %d at PC=%d\n", cpu->registers[64] + 1, cpu->registers[64]);
    }
}

void flush_BR(CPU *cpu, uint16_t new_pc, FILE *error_file) {
    if (new_pc < 0) {
        fprintf(error_file, "Branch is cancelled due to negative PC\n");
        return;
    }
    fprintf(error_file, "Flushing pipeline due to BR instruction with new PC value %d\n", new_pc);

    // Flush both IFID and IDEX registers and branch to new PC
    fprintf(error_file, "Flushing IFID and IDEX registers\n");

    // Flush IFID
    fprintf(error_file, "Flushing IFID: Instruction=0x%04X, Inst_Num=%d\n",
           cpu->IFID.instruction, cpu->IFID.inst_number);
    cpu->IFID.instruction = 0;  // Clear IFID register
    cpu->IFID.inst_number = 0;

    // Flush IDEX
    fprintf(error_file, "Flushing IDEX: Opcode=0x%X, RD=%d, RS1=%d, Immediate=0x%X, Inst_Num=%d\n",
           cpu->IDEX.opcode, cpu->IDEX.rd, cpu->IDEX.rs1, cpu->IDEX.immediate, cpu->IDEX.inst_number);
    erase_IDEX(cpu);

    cpu->registers[64] = new_pc - 1;

    if (cpu->registers[64] >= cpu->instruction_count) {
        fprintf(error_file, "Warning: Branch target out of bounds, PC=%d\n", cpu->registers[64]);
    } else {
        fprintf(error_file, "Branching to instruction %d at PC=%d\n", cpu->registers[64] + 1, cpu->registers[64]);
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

void execute(CPU *cpu, FILE *error_file) {
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
                    fprintf(error_file, "@PC=%d  BEQZ to %d: Branch Taken because R%d = %d\n", cpu->registers[64], imm, rd, cpu->registers[rd]);
                    flush(cpu, imm, error_file);  // Call flush function for branch
                } else {
                    fprintf(error_file, "BEQZ to %d: Branch Not Taken because R%d = %d\n", imm, rd, cpu->registers[rd]);
                }
                break;
            case 0x05: // ANDI
                if (imm >= 0 && imm <= 63) {
                    result = cpu->registers[rd] & imm;
                    cpu->registers[rd] = result;
                    update_status_register(cpu, result, rd, 0);
                } else {
                    fprintf(error_file, "Error: ANDI executed with invalid immediate value %d (valid range is 0-63)\n", imm);
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
                flush_BR(cpu, new_pc, error_file);
                break;
            }
            case 0x08: // SAL
                if (imm >= 0 && imm <= 63) {
                    result = cpu->registers[rd] << imm;
                    cpu->registers[rd] = result;
                    update_status_register(cpu, result, rd, 0);
                } else {
                    fprintf(error_file, "Error: SAL executed with invalid immediate value %d (valid range is 0-63)\n", imm);
                }
                break;
            case 0x09: // SAR
                if (imm >= 0 && imm <= 63) {
                    result = cpu->registers[rd] >> imm;
                    cpu->registers[rd] = result;
                    update_status_register(cpu, result, rd, 0);
                } else {
                    fprintf(error_file, "Error: SAR executed with invalid immediate value %d (valid range is 0-63)\n", imm);
                }
                break;
            case 0x0A: // LDR
                if (imm >= 0 && imm <= 63) {
                    result = cpu->data_memory[imm];
                    cpu->registers[rd] = result;
                    update_status_register(cpu, result, rd, 0);
                } else {
                    fprintf(error_file, "Error: LDR executed with invalid immediate value %d (valid range is 0-63)\n", imm);
                }
                break;
            case 0x0B: // STR
                if (imm >= 0 && imm <= 63) {
                    cpu->data_memory[imm] = cpu->registers[rd];
                } else {
                    fprintf(error_file, "Error: STR executed with invalid immediate value %d (valid range is 0-63)\n", imm);
                }
                break;
            default:
                fprintf(error_file, "Error: Unknown opcode 0x%X\n", opcode);
                break;
        }

        cpu->IDEX.isempty = 1;
        if (cpu->stall_flag != 1) {
            fprintf(error_file, "Executed Instruction %d: Opcode=0x%X, RD=%d, RS1=%d, Immediate=0x%X\n",
                   cpu->IDEX.inst_number, cpu->IDEX.opcode, cpu->IDEX.rd, cpu->IDEX.rs1, cpu->IDEX.immediate);
        }
    }
}

void write_cycle_data(CPU *cpu, int cycle, FILE *output_file) {
     fprintf(output_file, "Fetch: Instruction=0x%04X, PC=%d\n", cpu->IFID.instruction, cpu->registers[64] - 1);
    fprintf(output_file, "IFID: Instruction=0x%04X, PC=%d\n", cpu->IFID.instruction, cpu->registers[64]);
    fprintf(output_file, "IDEX: Opcode=0x%X, RD=%d, RS1=%d, Immediate=0x%X\n",
            cpu->IDEX.opcode, cpu->IDEX.rd, cpu->IDEX.rs1, cpu->IDEX.immediate);
    fprintf(output_file, "Execution: Opcode=0x%X, RD=%d, RS1=%d, Result=%d\n",
            cpu->IDEX.opcode, cpu->IDEX.rd, cpu->IDEX.rs1, cpu->registers[cpu->IDEX.rd]);
    fprintf(output_file, "Registers: PC=%d, SREG=0x%X\n", cpu->registers[64], cpu->registers[65]);
    fprintf(output_file, "DataMemory: ");
    for (int i = 0; i < DATA_MEMORY_SIZE; i++) {
        if (cpu->data_memory[i] != 0) {
            fprintf(output_file, "[%d:%d] ", i, cpu->data_memory[i]);
        }
    }
    fprintf(output_file, "\nInstructionMemory: ");
    for (int i = 0; i < INSTRUCTION_MEMORY_SIZE; i++) {
        if (cpu->instruction_memory[i].current_Instruction != 0) {
            fprintf(output_file, "[%d:0x%04X] ", i, cpu->instruction_memory[i].current_Instruction);
        }
    }
    fprintf(output_file, "\n\n");
}
