import tkinter as tk
from tkinter import filedialog, scrolledtext, ttk

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
        self.editor.bind("<Control-s>", self.save_file)

        # Menu for file operations
        menu_bar = tk.Menu(self)
        self.config(menu=menu_bar)

        file_menu = tk.Menu(menu_bar, tearoff=0)
        menu_bar.add_cascade(label="File", menu=file_menu)
        file_menu.add_command(label="Open", command=self.open_file)
        file_menu.add_command(label="Save", command=self.save_file)
        file_menu.add_command(label="Run", command=self.run_code)

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

    def setup_error_view(self):
        self.error_frame = tk.Frame(self)
        self.error_frame.pack(fill=tk.BOTH, expand=1, side=tk.BOTTOM)

        self.error_output = scrolledtext.ScrolledText(self.error_frame, wrap=tk.WORD, font=("Courier New", 10), height=10)
        self.error_output.pack(fill=tk.BOTH, expand=1, side=tk.BOTTOM)

    def open_file(self):
        file_path = filedialog.askopenfilename()
        if file_path:
            with open(file_path, 'r') as file:
                self.editor.delete(1.0, tk.END)
                self.editor.insert(tk.END, file.read())

    def save_file(self, event=None):
        file_path = filedialog.asksaveasfilename(defaultextension=".txt")
        if file_path:
            with open(file_path, 'w') as file:
                file.write(self.editor.get(1.0, tk.END))

    def run_code(self):
        # Save the code to a file and run the C backend
        self.save_file()

        # Assuming you have a script or makefile to compile and run the C program
        import subprocess
        subprocess.run("make -C ../cpu_backend", shell=True)
        subprocess.run("../cpu_backend/cpu_simulation", shell=True)

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
