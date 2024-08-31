import tkinter as tk
from tkinter import scrolledtext, ttk
import subprocess
import os

class CPUSimulatorIDE(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("CPU Simulator IDE")
        self.geometry("1000x700")

        # Initialize cycle data
        self.cycles = []
        self.final_cpu_state = None
        self.current_cycle = 0

        # Set up the editor and file management
        self.setup_editor()
        self.setup_console()
        self.setup_pipeline_view()
        self.setup_error_view()

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
        program_txt_path = "program.txt"
        if os.path.exists(program_txt_path):
            with open(program_txt_path, 'r') as file:
                self.editor.delete(1.0, tk.END)
                self.editor.insert(tk.END, file.read())

    def save_program_txt(self):
        # Clear the file before saving
        program_txt_path = "program.txt"
        with open(program_txt_path, 'w') as file:
            file.write("")  # Clear the content
        # Save the new content of the editor to program.txt
        with open(program_txt_path, 'w') as file:
            file.write(self.editor.get(1.0, tk.END))

    def clear_output_and_error_logs(self):
        # Clear the contents of the cycledata.txt and errorlog.txt files
        with open("cycledata.txt", 'w') as file:
            file.write("")  # Clear the content
        with open("errorlog.txt", 'w') as file:
            file.write("")  # Clear the content

    def run_code(self):
        # Save the current content of the editor to program.txt
        self.save_program_txt()

        # Clear output and error logs
        self.clear_output_and_error_logs()

        # Compile and run the C code
        c_file_path = "main.c"
        output_path = "cpu_simulation"

        compile_command = f"gcc {c_file_path} -o {output_path}"
        subprocess.run(compile_command, shell=True)

        # Run the compiled C program
        run_command = f"{output_path}"
        subprocess.run(run_command, shell=True)

        # Load the output data and error logs
        self.load_pipeline_data()
        self.load_error_log()

        # Parse the cycle data
        self.parse_cycle_data()

        # Display the first cycle by default
        if self.cycles:
            self.display_cycle(0)

    def load_pipeline_data(self):
        with open("cycledata.txt", "r") as file:
            self.console_output.delete(1.0, tk.END)
            self.console_output.insert(tk.END, file.read())

    def load_error_log(self):
        with open("errorlog.txt", "r") as file:
            self.error_output.delete(1.0, tk.END)
            self.error_output.insert(tk.END, file.read())

    def parse_cycle_data(self):
        with open("cycledata.txt", "r") as file:
            content = file.read()

        # Split the content into cycles using the new start and end markers
        self.cycles = []
        cycle_data = []
        recording = False

        for line in content.splitlines():
            if line.startswith("### START OF CYCLE"):
                cycle_data = [line]
                recording = True
            elif line.startswith("### END OF CYCLE"):
                cycle_data.append(line)
                self.cycles.append("\n".join(cycle_data))
                recording = False
            elif recording:
                cycle_data.append(line)
            elif line.startswith("Final CPU State:"):
                self.final_cpu_state = "\n".join(content.splitlines()[content.splitlines().index(line):])

    def display_cycle(self, cycle_index):
        if 0 <= cycle_index < len(self.cycles):
            self.console_output.delete(1.0, tk.END)
            self.console_output.insert(tk.END, self.cycles[cycle_index])
            self.current_cycle = cycle_index
            # Change the next button to "FINAL OUT" if on the last cycle
            if cycle_index == len(self.cycles) - 1 and self.final_cpu_state:
                self.next_button.config(text="FINAL OUT")
        elif cycle_index == len(self.cycles) and self.final_cpu_state:
            self.console_output.delete(1.0, tk.END)
            self.console_output.insert(tk.END, self.final_cpu_state)
            self.current_cycle = cycle_index

    def next_cycle(self):
        if self.current_cycle < len(self.cycles):
            self.display_cycle(self.current_cycle + 1)
        elif self.current_cycle == len(self.cycles) - 1 and self.final_cpu_state:
            self.display_cycle(self.current_cycle + 1)
            self.next_button.config(text="FINAL OUT")

    def prev_cycle(self):
        if self.current_cycle > 0:
            self.display_cycle(self.current_cycle - 1)
            self.next_button.config(text="Next Cycle")  # Reset text when going back

if __name__ == "__main__":
    app = CPUSimulatorIDE()
    app.mainloop()
