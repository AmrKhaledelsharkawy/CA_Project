import tkinter as tk
from tkinter import scrolledtext, ttk, filedialog, messagebox
import subprocess
import os
from ttkthemes import ThemedTk
from tkinter import font as tkFont

class LineNumbers(tk.Canvas):
    def __init__(self, *args, **kwargs):
        tk.Canvas.__init__(self, *args, **kwargs)
        self.textwidget = None

    def attach(self, text_widget):
        self.textwidget = text_widget

    def redraw(self, *args):
        self.delete("all")

        i = self.textwidget.index("@0,0")
        while True:
            dline = self.textwidget.dlineinfo(i)
            if dline is None: break
            y = dline[1]
            linenum = str(i).split(".")[0]
            self.create_text(2, y, anchor="nw", text=linenum, fill="#606366")
            i = self.textwidget.index(f"{i}+1line")

class CPUSimulatorIDE(ThemedTk):
    def __init__(self):
        super().__init__(theme="arc")
        self.title("CPU Simulator IDE")
        self.geometry("1200x800")

        self.cycles = []
        self.final_cpu_state = None
        self.current_cycle = 0
        self.dark_mode = False

        self.create_menu()
        self.setup_ui()
        self.load_program_txt()

        self.syntax_highlight()
        self.status_bar()

    def create_menu(self):
        menubar = tk.Menu(self)
        self.config(menu=menubar)

        file_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="File", menu=file_menu)
        file_menu.add_command(label="Open", command=self.open_file)
        file_menu.add_command(label="Save", command=self.save_file)
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=self.quit)

        edit_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Edit", menu=edit_menu)
        edit_menu.add_command(label="Cut", command=lambda: self.editor.event_generate("<<Cut>>"))
        edit_menu.add_command(label="Copy", command=lambda: self.editor.event_generate("<<Copy>>"))
        edit_menu.add_command(label="Paste", command=lambda: self.editor.event_generate("<<Paste>>"))

        view_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="View", menu=view_menu)
        view_menu.add_command(label="Toggle Dark Mode", command=self.toggle_dark_mode)

    def setup_ui(self):
        self.paned_window = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        self.paned_window.pack(fill=tk.BOTH, expand=1)

        left_frame = ttk.Frame(self.paned_window)
        right_frame = ttk.Frame(self.paned_window)

        self.paned_window.add(left_frame, weight=1)
        self.paned_window.add(right_frame, weight=1)

        self.setup_editor(left_frame)
        self.setup_output_views(right_frame)

    def setup_editor(self, parent):
        editor_frame = ttk.Frame(parent)
        editor_frame.pack(fill=tk.BOTH, expand=1)

        self.linenumbers = LineNumbers(editor_frame, width=30)
        self.linenumbers.pack(side=tk.LEFT, fill=tk.Y)

        self.editor = scrolledtext.ScrolledText(editor_frame, wrap=tk.WORD, font=("Consolas", 12), undo=True)
        self.editor.pack(fill=tk.BOTH, expand=1)
        self.editor.bind("<<Modified>>", self.on_text_change)

        self.linenumbers.attach(self.editor)

        button_frame = ttk.Frame(editor_frame)
        button_frame.pack(fill=tk.X)

        self.run_button = ttk.Button(button_frame, text="Run", command=self.run_code)
        self.run_button.pack(side=tk.LEFT, padx=5, pady=5)

    def setup_output_views(self, parent):
        notebook = ttk.Notebook(parent)
        notebook.pack(fill=tk.BOTH, expand=1)

        self.console_output = scrolledtext.ScrolledText(notebook, wrap=tk.WORD, font=("Consolas", 10), state='disabled')
        notebook.add(self.console_output, text="Console Output")

        self.error_output = scrolledtext.ScrolledText(notebook, wrap=tk.WORD, font=("Consolas", 10), state='disabled')
        notebook.add(self.error_output, text="Error Log")

        self.setup_pipeline_view(notebook)

    def setup_pipeline_view(self, notebook):
        pipeline_frame = ttk.Frame(notebook)
        notebook.add(pipeline_frame, text="Pipeline View")

        controls_frame = ttk.Frame(pipeline_frame)
        controls_frame.pack(fill=tk.X)

        self.prev_button = ttk.Button(controls_frame, text="Previous Cycle", command=self.prev_cycle)
        self.prev_button.pack(side=tk.LEFT, padx=5, pady=5)

        self.next_button = ttk.Button(controls_frame, text="Next Cycle", command=self.next_cycle)
        self.next_button.pack(side=tk.LEFT, padx=5, pady=5)

        self.cycle_label = ttk.Label(controls_frame, text="Cycle: 0/0")
        self.cycle_label.pack(side=tk.LEFT, padx=5, pady=5)

        self.pipeline_output = scrolledtext.ScrolledText(pipeline_frame, wrap=tk.WORD, font=("Consolas", 10), state='disabled')
        self.pipeline_output.pack(fill=tk.BOTH, expand=1)

    def load_program_txt(self):
        program_txt_path = "program.txt"
        if os.path.exists(program_txt_path):
            with open(program_txt_path, 'r') as file:
                self.editor.delete(1.0, tk.END)
                self.editor.insert(tk.END, file.read())

    def save_program_txt(self):
        program_txt_path = "program.txt"
        with open(program_txt_path, 'w') as file:
            file.write(self.editor.get(1.0, tk.END))

    def clear_output_and_error_logs(self):
        for file_path in ["cycledata.txt", "errorlog.txt"]:
            with open(file_path, 'w') as file:
                file.write("")

    def run_code(self):
        self.save_program_txt()
        self.clear_output_and_error_logs()
        self.reset_view()

        c_file_path = "main.c"
        output_path = "./cpu_simulation"

        compile_command = f"gcc {c_file_path} -o {output_path}"
        compile_result = subprocess.run(compile_command, shell=True, capture_output=True, text=True)

        if compile_result.returncode != 0:
            self.update_error_output(compile_result.stderr)
            return

        run_command = output_path
        run_result = subprocess.run(run_command, shell=True, capture_output=True, text=True)

        if run_result.returncode != 0:
            self.update_error_output(run_result.stderr)
            return

        self.load_pipeline_data()
        self.load_error_log()
        self.parse_cycle_data()

        if self.cycles:
            self.display_cycle(0)

    def load_pipeline_data(self):
        with open("cycledata.txt", "r") as file:
            self.update_console_output(file.read())

    def load_error_log(self):
        with open("errorlog.txt", "r") as file:
            self.update_error_output(file.read())

    def parse_cycle_data(self):
        with open("cycledata.txt", "r") as file:
            content = file.read()

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
            self.update_pipeline_output(self.cycles[cycle_index])
            self.current_cycle = cycle_index
            self.update_cycle_label()
        elif cycle_index == len(self.cycles) and self.final_cpu_state:
            self.update_pipeline_output(self.final_cpu_state)
            self.current_cycle = cycle_index
            self.update_cycle_label()

    def next_cycle(self):
        if self.current_cycle < len(self.cycles):
            self.display_cycle(self.current_cycle + 1)

    def prev_cycle(self):
        if self.current_cycle > 0:
            self.display_cycle(self.current_cycle - 1)

    def update_console_output(self, text):
        self.console_output.config(state='normal')
        self.console_output.delete(1.0, tk.END)
        self.console_output.insert(tk.END, text)
        self.console_output.config(state='disabled')

    def update_error_output(self, text):
        self.error_output.config(state='normal')
        self.error_output.delete(1.0, tk.END)
        self.error_output.insert(tk.END, text)
        self.error_output.config(state='disabled')

    def update_pipeline_output(self, text):
        self.pipeline_output.config(state='normal')
        self.pipeline_output.delete(1.0, tk.END)
        self.pipeline_output.insert(tk.END, text)
        self.pipeline_output.config(state='disabled')

    def update_cycle_label(self):
        self.cycle_label.config(text=f"Cycle: {self.current_cycle + 1}/{len(self.cycles)}")

    def reset_view(self):
        self.cycles = []
        self.final_cpu_state = None
        self.current_cycle = 0
        self.update_console_output("")
        self.update_error_output("")
        self.update_pipeline_output("")
        self.update_cycle_label()

    def toggle_dark_mode(self):
        self.dark_mode = not self.dark_mode
        style = ttk.Style()
        if self.dark_mode:
            self.set_theme("equilux")
            style.configure("TNotebook", background="#2f2f2f")
            style.configure("TNotebook.Tab", background="#2f2f2f", foreground="white")
            self.editor.config(bg="#2f2f2f", fg="white", insertbackground="white")
            self.console_output.config(bg="#2f2f2f", fg="white")
            self.error_output.config(bg="#2f2f2f", fg="white")
            self.pipeline_output.config(bg="#2f2f2f", fg="white")
            self.linenumbers.config(bg="#2f2f2f", fg="#606366")
        else:
            self.set_theme("arc")
            style.configure("TNotebook", background="#f5f6f7")
            style.configure("TNotebook.Tab", background="#f5f6f7", foreground="black")
            self.editor.config(bg="white", fg="black", insertbackground="black")
            self.console_output.config(bg="white", fg="black")
            self.error_output.config(bg="white", fg="black")
            self.pipeline_output.config(bg="white", fg="black")
            self.linenumbers.config(bg="#f0f0f0", fg="#606366")
        
        # Apply syntax highlighting after toggling the mode
        self.syntax_highlight()

    def open_file(self):
        file_path = filedialog.askopenfilename(filetypes=[("Text files", "*.txt"), ("All files", "*.*")])
        if file_path:
            with open(file_path, 'r') as file:
                self.editor.delete(1.0, tk.END)
                self.editor.insert(tk.END, file.read())

    def save_file(self):
        file_path = filedialog.asksaveasfilename(defaultextension=".txt", filetypes=[("Text files", "*.txt"), ("All files", "*.*")])
        if file_path:
            with open(file_path, 'w') as file:
                file.write(self.editor.get(1.0, tk.END))

    def syntax_highlight(self):
        self.editor.tag_remove("keyword", "1.0", tk.END)
        self.editor.tag_remove("operator", "1.0", tk.END)
        self.editor.tag_remove("string", "1.0", tk.END)
        self.editor.tag_remove("comment", "1.0", tk.END)
        self.editor.tag_remove("register_error", "1.0", tk.END)

        operations = {"MOVI", "ADD", "SUB", "MUL", "BEQZ", "ANDI", "EOR", "BR", "SAL", "SAR", "LDR", "STR"}
        registers = {f"R{i}" for i in range(1, 64)}
        registers.add("R0")  # R0 is included for error highlighting.

        operation_color = "#569cd6" if self.dark_mode else "#0000FF"
        register_color = "#dcdcaa" if self.dark_mode else "#800080"
        error_color = "#FF0000"  # Red for R0 to flag as an error

        self.highlight_pattern(operations, "keyword", operation_color)
        self.highlight_pattern(registers, "operator", register_color)
        self.highlight_pattern(["R0"], "register_error", error_color)

        if "R0" in self.editor.get(1.0, tk.END):
            self.show_r0_error()

    def highlight_pattern(self, patterns, tag, color):
        for pattern in patterns:
            start = "1.0"
            while True:
                start = self.editor.search(r'\b' + pattern + r'\b', start, tk.END, regexp=True)
                if not start:
                    break
                end = f"{start}+{len(pattern)}c"
                self.editor.tag_add(tag, start, end)
                self.editor.tag_config(tag, foreground=color)
                start = end

    def highlight_regex(self, pattern, tag, color):
        start = "1.0"
        while True:
            start = self.editor.search(pattern, start, tk.END, regexp=True)
            if not start:
                break
            end = f"{start}+{len(self.editor.get(start, f'{start} lineend'))}c"
            self.editor.tag_add(tag, start, end)
            self.editor.tag_config(tag, foreground=color)
            start = end

    def show_r0_error(self):
        messagebox.showwarning("Warning", "R0 is not allowed. Please use a different register.")

    def on_text_change(self, event=None):
        self.linenumbers.redraw()
        self.syntax_highlight()  # Ensure that syntax highlighting is applied on text change
        self.editor.edit_modified(False)

    def status_bar(self):
        status = ttk.Label(self, text="CPU Simulator IDE - Ready", anchor=tk.W)
        status.pack(side=tk.BOTTOM, fill=tk.X)


if __name__ == "__main__":
    app = CPUSimulatorIDE()
    app.mainloop()
