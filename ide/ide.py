import tkinter as tk
from tkinter import scrolledtext, ttk
import subprocess
import os

class CPUSimulatorIDE(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("CPU Simulator IDE")
        self.geometry("1000x700")

        # Set up the editor and file management
        self.setup_editor()
        self.setup_console()
        self.setup_pipeline_view()
        self.setup_error_view()
        self.current_cycle = 0

    def setup_editor(self):
        self.editor = scrolledtext.ScrolledText(self, wrap=tk.WORD, font=("Courier New", 12))
        self.editor.pack(fill=tk.BOTH, expand=1, side=tk.TOP)
        self.load_program_txt()

    def setup_console(self):
        self.console_output = scrolledtext.ScrolledText(self, wrap=tk.WORD, font=("Courier New", 10), height=10)
        self.console_output.pack(fill=tk.X, side=tk.BOTTOM)

    def setup_pipeline_view(self):
        self.pipeline_frame = tk.Frame(self)
        self.pipeline_frame.pack(fill=tk.BOTH, expand=1, side=tk.BOTTOM)

        # Dropdown menus for registers, data memory, and instruction memory
        self.registers_var = tk.StringVar(self)
        self.data_memory_var = tk.StringVar(self)
        self.instruction_memory_var = tk.StringVar(self)

        self.registers_dropdown = ttk.Combobox(self.pipeline_frame, textvariable=self.registers_var)
        self.registers_dropdown.pack(side=tk.LEFT, padx=5, pady=5)

        self.data_memory_dropdown = ttk.Combobox(self.pipeline_frame, textvariable=self.data_memory_var)
        self.data_memory_dropdown.pack(side=tk.LEFT, padx=5, pady=5)

        self.instruction_memory_dropdown = ttk.Combobox(self.pipeline_frame, textvariable=self.instruction_memory_var)
        self.instruction_memory_dropdown.pack(side=tk.LEFT, padx=5, pady=5)

        # Buttons to navigate cycles
        self.prev_button = tk.Button(self.pipeline_frame, text="Previous Cycle", command=self.prev_cycle)
        self.prev_button.pack(side=tk.LEFT, padx=5, pady=5)

        self.next_button = tk.Button(self.pipeline_frame, text="Next Cycle", command=self.next_cycle)
        self.next_button.pack(side=tk.LEFT, padx=5, pady=5)

        # Run button
        self.run_button = tk.Button(self.pipeline_frame, text="Run", command=self.run_code)
        self.run_button.pack(side=tk.LEFT, padx=5, pady=5)

    def setup_error_view(self):
        self.error_frame = tk.Frame(self)
        self.error_frame.pack(fill=tk.BOTH, expand=1, side=tk.BOTTOM)

        self.error_output = scrolledtext.ScrolledText(self.error_frame, wrap=tk.WORD, font=("Courier New", 10), height=10)
        self.error_output.pack(fill=tk.BOTH, expand=1, side=tk.BOTTOM)

    def load_program_txt(self):
        program_txt_path = "../cpu_backend/program.txt"
        if os.path.exists(program_txt_path):
            with open(program_txt_path, 'r') as file:
                self.editor.delete(1.0, tk.END)
                self.editor.insert(tk.END, file.read())

    def run_code(self):
        # Compile and run the C code
        c_file_path = "../cpu_backend/main.c"
        output_path = "../cpu_backend/cpu_simulation"

        compile_command = f"gcc {c_file_path} -o {output_path}"
        subprocess.run(compile_command, shell=True)

        # Run the compiled C program
        run_command = f"{output_path}"
        subprocess.run(run_command, shell=True)

        # Load the output data
        self.load_pipeline_data()
        self.load_error_log()

    def load_pipeline_data(self):
        with open("../output/cycle_data.txt", "r") as file:
            self.console_output.delete(1.0, tk.END)
            self.console_output.insert(tk.END, file.read())

    def load_error_log(self):
        with open("../output/error_log.txt", "r") as file:
            self.error_output.delete(1.0, tk.END)
            self.error_output.insert(tk.END, file.read())

    def next_cycle(self):
        # Logic to fetch and display the next cycle
        self.current_cycle += 1
        # Implement the logic to fetch data from cycle_data.txt

    def prev_cycle(self):
        # Logic to fetch and display the previous cycle
        if self.current_cycle > 0:
            self.current_cycle -= 1
            # Implement the logic to fetch data from cycle_data.txt


if __name__ == "__main__":
    app = CPUSimulatorIDE()
    app.mainloop()
