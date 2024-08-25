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

typedef struct { // instruction will be short int (16 bits) -- PC will be int (32 bits)
    uint16_t instruction_memory[INSTRUCTION_MEMORY_SIZE];
    uint8_t data_memory[DATA_MEMORY_SIZE];
    uint8_t registers[REGISTER_COUNT];
    uint16_t pc;
    PipelineStage fetch, decode, execute;
    uint8_t sreg; // Status Register
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
fetchedInstruction CurrentInstruction = {0};
DecodedInstruction IR = {0, 0, 0, 0};

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

//Marwan ALU approach: 
//int ALU(int operation, int operandA, int operandB)
//{
	//int output = 0;
	//zeroFlag = 0;
	//switch (operation)
	//{

	//case 0: // ADD
	//	output = (int)(unsigned int)operandA + operandB;
	//	break;

	//case 1: // SUB
	//	output = (int)(unsigned int)operandA - operandB;
	//	break;

	//case 2: // MUL
	//	output = (int)(unsigned int)operandA * operandB;
	//	break;

    //default:
    //  break;
    //}

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
    // TODO: Implement CPU initialization
}

void load_program(CPU *cpu, const char *filename) {
    // TODO: Implement program loading
}

void print_cpu_state(CPU *cpu) {
    // TODO: Implement CPU state printing
}

void run_pipeline(CPU *cpu) {
    // TODO: Implement pipeline running
}

void fetch(CPU *cpu) {
    // TODO: Implement fetch stage
}

void decode(CPU *cpu) {
    // TODO: Implement decode stage
}

void execute(CPU *cpu) {
    // TODO: Implement execute stage
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
    // TODO: Implement BEQZ instruction
}

void execute_andi(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement ANDI instruction
}

void execute_eor(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement EOR instruction
}

void execute_br(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement BR instruction
}

void execute_sal(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement SAL instruction
}

void execute_sar(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement SAR instruction
}

void execute_ldr(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement LDR instruction
}

void execute_str(CPU *cpu, DecodedInstruction instr) {
    // TODO: Implement STR instruction
}
